# 導電線路 CAE 訓練

這份程式用卷積自編碼器（Convolutional Autoencoder, CAE）學習「正常導電線路」的光譜樣貌。重建誤差大的位置視為偏離正常外觀，再把遮罩內的平均誤差換成 0–100 的均勻度。

日常訓練與推論用這三支程式，都在本資料夾執行：

| 檔案 | 用途 |
| --- | --- |
| `auto_sam_mask.py` | 由校正後的 ENVI 影像自動圈出導電線路，先檢查遮罩再訓練 |
| `train_cae.py` | 擬合 PCA、切 patch、訓練 CAE、輸出殘差與均勻度 |
| `infer_cae.py` | 載入已訓練的 run，對新影像算均勻度 |

`cae_runs/` 裡的 `finetune_site16_oof*.py` 是現場 16 張樣本的既有實驗腳本，路徑寫死在當時的 bundle，不能直接當成一般訓練入口。

## 原理

整條流程是：校正後光譜立方 → 線路遮罩 → 主成分降維 → 遮罩內 patch 重建 → 光譜空間殘差 → 均勻度。

### 1. 只在線路上計算誤差

背景與基材不參與損失。程式先用平均反射率把樣品矩形從暗背景分開，再用光譜角把導電線路從基材裡圈出來。自編碼器的損失、殘差與均勻度都乘上這張線路遮罩。

這裡的 SAM 是 Spectral Angle Mapper（光譜角映射）：每個像素的光譜向量與線路種子的平均光譜算夾角，夾角小的像素歸入線路。

### 2. 遮罩怎麼產生

`find_label_mask` 與 `auto_sam_mask.py` 用同一套規則。

1. 對全波段平均影像做 Otsu，亮區當成樣品，再做閉運算，並去掉貼在感測器左右邊緣的高條狀校正假影。
2. 把樣品遮罩侵蝕出核心區。在核心區再做一次 Otsu，分成較亮與較暗兩群。`auto` 會選面積較小的那一群當線路極性（亮線路或暗線路）。
3. 在核心區取該極性的高分位像素當種子，預設第 85 百分位。種子光譜的平均就是參考光譜。
4. 全圖計算與參考光譜的夾角。門檻預設再用 Otsu；若線路占樣品面積超過 `--max-label-fraction`（預設 0.32），就改用該比例的百分位，避免遮罩擴到整片基材。

現場 fine-tune 在這張初始遮罩之後，只保留最大的 8-connected component，零星孤立像素不進訓練。

### 3. PCA

線路像素的光譜維度仍高（預設兩端各裁 10 個波段後約 204 波段）。PCA 只在 **train** 切分的線路像素上擬合：

1. 每張訓練影像抽線路像素，總數預設不超過 80,000。
2. 逐波段減平均、除標準差。標準差過小的波段改除 1，避免除以零。
3. 對標準化後的像素矩陣做 SVD，保留前 `--pca-components` 個主成分，預設 8。
4. 投影公式是 `((光譜 - band_mean) / band_std) @ components.T`。

推論必須沿用訓練時存下的 `pca_model.npz`。現場 fine-tune 故意不重擬合 PCA，這樣新模型與基礎模型的特徵空間相同。

### 4. 卷積自編碼器

每張影像依線路外接範圍，以 32×32、stride 8 切 patch。遮罩占比低於 0.15 的 patch 不進訓練。

模型輸入是 PCA 通道，並可再接一層遮罩通道（預設開啟）。遮罩外的特徵在送進網路前先乘 0。重建目標是未乘遮罩的 PCA patch。損失是遮罩內的均方誤差：

```text
masked MSE = sum((pred - target)^2 * mask) / (mask 像素數 × 通道數)
```

網路是四段編碼、兩次 stride-2 下採樣，再以轉置卷積解碼回原解析度。每個區塊是 3×3 卷積、GroupNorm、SiLU。通道數由 `--base-channels` 起算，預設 32、64、128、128。

最佳化用 AdamW。有驗證集時，以驗證損失做 ReduceLROnPlateau（patience 5、學習率減半），並在 `--patience`（預設 12）個 epoch 沒有改善時提早停止。沒有驗證集時，每個 epoch 都會覆寫 `best_cae.pt`，最後留下的是最終 epoch。CUDA 上預設開啟混合精度與 channels-last。

同一張影像的 patch 不會同時出現在 train 與 val。切分以影像為單位，並依檔名前綴的厚度（`50u`、`70u`）分層。

### 5. 殘差與均勻度

評估時用 stride 16 的滑動視窗重建，重疊處對誤差取平均。預設 `--residual-space spectral`：把重建的 PCA 向量乘回 `components`，還原成光譜後，對波段取均方誤差。`pca` 則直接在 PCA 通道上取均方誤差。

校正門檻 `calibration_pixel_p99` 來自校正切分（有 val 就用 val，否則用 train）全部線路像素殘差的第 99 百分位。對每一張影像：

```text
hot_area_ratio = 線路像素中，殘差 > calibration_pixel_p99 的比例
uniformity     = 100 × (1 - min(平均殘差 / calibration_pixel_p99, 1))
```

平均殘差愈接近 0，均勻度愈接近 100。平均殘差達到或超過校正 p99 時，均勻度為 0。

## 準備資料

每張樣本是一對 ENVI 檔，檔名以 `_RT` 結尾：

```text
50u_1_RT.hdr
50u_1_RT.raw
70u_3_RT.hdr
70u_3_RT.raw
```

要求：

- interleave 必須是 `BIL`。header 需要 `samples`、`lines`、`bands`、`data type`。
- `.raw` 與 `.hdr` 同名，放在同一資料夾。
- 檔名第一個底線前的文字就是厚度標籤。`--thickness 50u` 只收 `50u_` 開頭的檔案；`all` 會把每種前綴各自分層切分。
- 反射率立方應已完成黑白校正。訓練程式不負責拍攝校正檔。
- 預設兩端各去掉 10 個波段（`--trim-bands 10`），用來避開感測器邊緣波段。

下列檔案的 RAW 位元組數與 header 的 `lines` 對不上，預設排除。確認 header 修正後，才加 `--include-excluded`：

```text
50u_15_RT
70u_1_RT
70u_2_RT
70u_6_RT
70u_7_RT
70u_12_RT
```

未指定 `--data-dir` 時，程式依序尋找：

1. `CAE訓練/導電材料_RT`
2. 上一層的 `20260528_導電材料/導電材料_RT`

既有基礎模型 `cae_runs/cae_all_v3` 是用 `導電材料_RT_v3` 訓練的，當時加了 `--include-excluded`，且驗證比例為 0。

訓練前先產生遮罩預覽，確認黃線蓋在線路上，而不是整片基材或背景：

```powershell
python auto_sam_mask.py --data-dir "D:\data\導電材料_RT" --all --output-dir sam_mask_outputs
```

每張影像會在 `sam_mask_outputs/<檔名>/` 留下 `pseudo_rgb.png`、`sam_score.png`、`sam_mask.png`、`sam_overlay.png`、`sam_mask.npy` 與 `report.html`。極性反了可改 `--label-polarity bright` 或 `dark`；遮罩太大就降低 `--max-label-fraction`。

每種厚度至少要有 3 張影像，否則一般切分會失敗。使用 k-fold 時，每種厚度的張數還必須不少於 fold 數。

## 訓練

在 `CAE訓練` 資料夾、已安裝 NumPy 與 CUDA 版 PyTorch 的環境執行。評估圖若要中文標題，建議另外安裝 Matplotlib；沒有它會改用內建點陣字畫英文摘要圖。

50 µm 與 70 µm 一起訓練、保留驗證集與測試集：

```powershell
python train_cae.py --data-dir "D:\data\導電材料_RT" --run-name cae_all_v4 --thickness all --device cuda
```

只訓練一種厚度：

```powershell
python train_cae.py --data-dir "D:\data\導電材料_RT" --run-name cae_50u --thickness 50u --device cuda
```

常用參數的預設值：

| 參數 | 預設 | 意義 |
| --- | --- | --- |
| `--val-ratio` / `--test-ratio` | 0.15 / 0.15 | 每種厚度內的影像比例。設成 0 表示該切分為空 |
| `--seed` | 42 | 切分、抽樣與 PyTorch 的亂數種子 |
| `--pca-components` | 8 | 保留的主成分數 |
| `--max-pca-pixels` | 80000 | 擬合 PCA 的線路像素上限 |
| `--patch-size` / `--patch-stride` | 32 / 8 | 訓練 patch |
| `--eval-stride` | 16 | 評估時的滑動視窗步距 |
| `--epochs` | 80 | 最大 epoch |
| `--batch-size` | 256 | |
| `--lr` / `--weight-decay` | 1e-3 / 1e-5 | AdamW |
| `--patience` | 12 | 驗證損失未改善就停止 |
| `--base-channels` | 32 | 編碼器第一層通道數 |

五折交叉驗證時，第 k 折把 `index % 5 == k` 的影像放到 val，其餘為 train，沒有獨立 test。patch 陣列很大，交叉驗證建議關掉存檔：

```powershell
python train_cae.py --data-dir "D:\data\導電材料_RT" --run-name cae_cv_fold0 --cv-fold-index 0 --cv-fold-count 5 --no-save-patches --device cuda
```

`--skip-eval` 只訓練、不算殘差圖。全部影像都拿去擬合最終權重時，把兩個比例設為 0；此時 `best_cae.pt` 是最後一個 epoch。

一次成功的 run 寫在 `cae_runs/<run-name>/`：

| 檔案 | 內容 |
| --- | --- |
| `config.json` | 參數目錄與每種厚度的張數 |
| `split_manifest.json` / `.csv` | 每張影像屬於 train、val 或 test |
| `pca_model.npz` | `band_mean`、`band_std`、`components`、解釋變異比 |
| `best_cae.pt` | 權重，以及輸入通道、patch 大小、最佳 epoch |
| `history.csv` | 每個 epoch 的 train / val loss |
| `patches_*.npz` | 可選的 patch 陣列 |
| `mask_previews/` | 每張影像的遮罩疊圖 |
| `residual_outputs/` | 殘差熱圖、遮罩、`result_summary.png`、`residual_map.npy` |
| `residual_metrics.csv` | 平均殘差、p95、校正 p99、熱區比例、均勻度 |

## 推論

指定某一次訓練的資料夾，以及單張 header 或整批影像：

```powershell
python infer_cae.py --run-dir cae_runs/cae_all_v3 --data-dir "D:\data\導電材料_RT" --file 50u_1_RT.hdr --device cuda
python infer_cae.py --run-dir cae_runs/cae_all_v3 --data-dir "D:\data\導電材料_RT" --all --device cuda
```

程式會讀該 run 的 `config.json`、`pca_model.npz`、`best_cae.pt`。校正 p99 依序使用 `--calibration-p99`、`residual_metrics.csv` 裡的中位數，或重新跑 val 切分來估計。結果預設寫到該 run 的 `inference_outputs/`，並彙總成 `inference_metrics.csv`。

若要依厚度把影像送到不同模型，省略 `--run-dir` 或加上 `--auto-thickness`。厚度來源由 `--thickness-source` 決定：

- `predict`：使用 `電阻值模型/thickness_classifier.py` 的分類器，預設模型路徑見該檔的 `DEFAULT_THICKNESS_MODEL`。
- `filename`：從 `50u_`、`70u_` 這類檔名解析。
- `auto`：檔名解析得到就用檔名，否則用分類器。

分類信心低於 `--min-thickness-confidence`（預設 0.75）時，`--low-confidence-policy` 預設 `dual-cae`：50u 與 70u 兩個模型都跑，取得分較低（正規化平均誤差、熱區比例、平均殘差）的那個。`all-model` 改走 `--run-dir-all`，`use-classifier` 仍採用分類器給出的厚度。

## 既有的現場 fine-tune

`cae_runs/cae_all_v3` 是目前基礎模型：50u、70u 各 15 張，全部用於訓練，PCA 為 8 維，約 79 個 epoch。

`cae_runs/cae_site16_ft_4fold_oof` 與其 `50ep` 版本，是在這個基礎權重上，用現場 FX17_0001–0017 中排除 FX17_0003、FX17_0018 的 16 張做 fine-tune。PCA 不重擬合。4-fold 以樣本為單位，每折 12 張訓練、4 張驗證。30 epoch 那次的學習率是 1e-4、batch 256、weight decay 1e-5；四折的 OOF pooled mean residual 約從 3.40e-5 降到 1.96e-5。細節見 `cae_runs/cae_site16_ft_4fold_oof/FINETUNE_REPORT.md`。

這兩個 fine-tune 腳本裡的資料根目錄指向當時的 `FX17_site_bundle_0812`。換電腦執行前要先改那些絕對路徑。正式換現場模型時，CAE run 與對應的 calibration JSON 要一起換，不能只換 `best_cae.pt`。

## 程式碼重點

- 遮罩、PCA 與網路是三段分開的處理。遮罩品質會直接決定 PCA 抽到什麼光譜，也決定損失看哪些像素。
- `load_envi_bil` 以 RAW 檔實際位元組數決定行數。header 的 `lines` 不一致時只警告並採用 RAW 行數；除不盡的檔案會直接失敗。
- 輸入通道數是 PCA 維度再加 1 層遮罩。推論時的 `best_cae.pt` 必須和產生它的 `pca_model.npz`、`include_mask_channel` 設定成套使用。
- 殘差圖把重疊視窗的誤差加總後除以覆蓋次數，遮罩外設為 NaN，熱圖只畫線路內部。
- Windows 上含中文路徑時，權重經 Python 檔案物件存取，避開舊版 `torch.save` 對路徑的限制。
- `infer_cae.py` 會把 `電阻值模型` 加進 `sys.path`，厚度自動分流才需要那支分類器；指定 `--run-dir` 的單模型推論不依賴它。

## 函式介紹

下列是三支主程式的對外函式。原始碼裡的 JSDoc 與這份說明對應。

### `auto_sam_mask.py`

| 函式 | 作用 |
| --- | --- |
| `parse_envi_header` | 讀 ENVI header，取出尺寸、型別、interleave 與波長 |
| `raw_path_for_header` | 把 `.hdr` 換成同名 `.raw`，找不到就失敗 |
| `load_envi_bil` | 以 BIL 讀立方，裁掉兩端波段，轉成 lines × samples × bands 的 float32 |
| `normalize01` | 以遮罩內 1%–99% 分位把數值壓到 0–1 |
| `otsu_threshold` | 以類間變異最大的門檻做二值化 |
| `erode_mask` / `dilate_mask` / `clean_mask` | 3×3 侵蝕、膨脹，以及先膨脹再侵蝕的閉運算 |
| `suppress_edge_bands` | 去掉貼在左右感測器邊緣、幾乎貫穿整列的假影 |
| `build_sample_mask` | 由平均影像得到樣品矩形遮罩 |
| `choose_label_seed` | 在樣品核心決定亮／暗極性，並取出高信心種子 |
| `sam_degrees` | 計算每個像素與參考光譜的夾角（度） |
| `choose_sam_mask` | 依夾角門檻得到線路遮罩，並限制它在樣品中的面積比例 |
| `wavelength_index` | 找最接近目標波長的波段；沒有波長表時用比例位置 |
| `pseudo_rgb` | 用約 1650、1350、1100 nm 組成偽色圖 |
| `apply_colormap` / `mask_to_yellow` / `overlay_mask` | 把分數或遮罩畫成可檢查的顏色圖 |
| `write_png` | 不依賴影像套件，直接寫 8-bit 灰階或 RGB PNG |
| `write_report` | 寫單張樣本的 `report.html` |
| `process_one` | 對單一 header 跑完整遮罩並存預覽 |
| `list_headers` | 列出 `*_RT.hdr`，預設跳過行數不一致的檔案 |
| `main` | 命令列入口 |

### `train_cae.py`

| 函式 | 作用 |
| --- | --- |
| `ImageRecord` | 一張影像的檔名、厚度、header 路徑與切分 |
| `PcaModel` | 波段平均、標準差、主成分與解釋變異比 |
| `PatchDataset` | 把 input、target、mask 三個陣列包成 PyTorch dataset |
| `discover_images` | 依厚度與排除名單收集 `*_RT.hdr` |
| `stratified_split` | 每種厚度內依比例切 train / val / test |
| `stratified_kfold` | 每種厚度內做樣本層級的 k-fold，指定折為 val |
| `find_label_mask` | 呼叫遮罩流程，回傳布林遮罩與種子、門檻等摘要 |
| `sample_label_pixels` | 從訓練影像的線路像素抽樣，供 PCA 使用 |
| `fit_pca` | SVD 擬合 PCA，並把模型與遮罩摘要寫進 run |
| `transform_cube_to_pca` | 把整張立方投影成 lines × samples × 主成分 |
| `patch_positions` / `extract_patches_from_image` | 在線路附近切 patch，並組成帶遮罩通道的輸入 |
| `build_patch_arrays` | 逐張影像產生三個切分的 patch，並存遮罩預覽 |
| `make_model` | 建立 ConvBlock 編碼器與轉置卷積解碼器 |
| `masked_mse` | 只在遮罩內計算通道平均的均方誤差 |
| `run_epoch` | 跑一個訓練或驗證 epoch，含混合精度與梯度裁剪 |
| `train_model` | 訓練循環、學習率排程、早期停止，寫出 `best_cae.pt` |
| `residual_map_for_image` | 滑動視窗重建，匯總成一張殘差圖 |
| `evaluate_model` | 先收集校正 p99，再為每張影像存熱圖與 `residual_metrics.csv` |
| `main` | 切分、PCA、訓練、評估的命令列入口 |

`draw_text`、`residual_to_heatmap`、`save_result_summary` 與 `save_residual_images` 負責沒有 Matplotlib 時的摘要圖，以及殘差熱圖、遮罩圖的存檔。

### `infer_cae.py`

| 函式 | 作用 |
| --- | --- |
| `load_run_args` | 合併 run 的 `config.json` 與命令列覆寫 |
| `load_pca` / `load_model` | 載入 `pca_model.npz` 與 `best_cae.pt` |
| `calibration_from_metrics` | 從既有 `residual_metrics.csv` 取校正 p99 的中位數 |
| `calibration_from_val_split` | 指標檔不在時，重算 val 影像殘差的第 99 百分位 |
| `choose_inputs` | 決定要跑單一 `--file` 或 `--all` |
| `infer_one` | 對一張 header 產生殘差、均勻度與輸出圖 |
| `resolve_auto_thickness` | 由檔名或厚度分類器決定 50u / 70u |
| `confidence_is_low` / `candidate_score` | 判斷分類信心，並在兩個 CAE 候選之間選誤差較小者 |
| `run_candidate` | 載入並執行某一個厚度模型 |
| `load_inference_bundle` | 一次準備參數、PCA、權重與校正 p99 |
| `main` | 單模型推論，或依厚度自動分流 |
