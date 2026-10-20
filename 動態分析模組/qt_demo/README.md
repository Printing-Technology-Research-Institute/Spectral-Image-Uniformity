# 光譜影像均勻度動態分析軟體

目前版本可由 `settings.example.json` 的 `stream_source` 選擇 `camera` 或
`recorded`。上方顯示最新一幀 `光譜 × 空間` detector 畫面，左側以
1650/1300/1000 nm 組成逐行即時偽色影像；自動偵測到完整樣本後，由常駐
背景 GPU worker 即時計算熱圖、均勻度及電阻預測。

`recorded` 與 `camera` 共用 Qt、校正、邊界偵測及分析流程，只替換 stream
worker。共用訊息與 worker 介面定義在 `stream_source.py`，共用 raw-line
處理位於 `raw_line_pipeline.py`，實機來源位於 `camera_stream.py`；沒有複製
另一套介面程式。

## 執行

```powershell
cd C:\Users\wu\Desktop\產學\動態分析模組\qt_demo
python app.py
```

相機控制區的「開始／停止」會暫停／恢復目前選定的 stream worker。
`camera` worker 依 FX17 範例在連線後設定
`EnAcquisitionFrameRate=False`、`Mono12Packed` 與介面上的曝光值；開始
acquisition 後開快門，暫停、例外或關閉時在 `finally` 中關閉快門。
`camera_configure_on_connect`、`camera_open_shutter_on_start` 與
`camera_close_shutter_on_pause` 可在設定檔控制。校正拍攝按鈕仍未送出真實
硬體命令。

## 檢測分析輸出

在「檢測分析」區先選擇分析結果儲存路徑。每次按下「開始」會建立一個
`檢測紀錄_YYYYMMDD_HHMMSS` 工作階段資料夾；每個完整樣本再依樣本偵測開始
時間建立 `YYYYMMDD_HHMMSS_ffffff` 子資料夾。資料夾內包含：

- `拍攝時間_未校正.raw/.hdr/.json`
- `拍攝時間_校正.raw/.hdr/.json`
- `拍攝時間_殘差熱圖.png`（圖內含均勻度與預測電阻值）

工作階段根目錄的 Excel 會逐張記錄拍攝開始/結束時間、檔名、樣本 ID、
均勻度、電阻值、曝光時間、line rate、黑白校正檔、樣本行數、分析時間與
狀態。黑白校正檔會在按下開始時複製到工作階段的 `校正檔` 資料夾，避免
日後原路徑變更造成紀錄無法追溯。按下「停止」後，已完整拍攝的樣本仍會
完成分析與存檔，再關閉該工作階段。

顯示端使用同一個 rolling line buffer，只顯示截至當下收到的影像行。
`camera` 來源會把每批 GenTL 相機行送入同一個 buffer；`recorded` 則送入
錄製的未校正 RAW 行。

`app.py` 直接讀取 30 組原始未校正 uint16 BIL，不讀取 v3 HDR，也不使用 manifest 內預先算好的樣本終點。每一批原始行都會即時執行黑白校正、Material score 與邊界狀態機；`SAMPLE_END` 後把當次收集的 cube 直接提交 GPU worker。

## 模擬演算法

- CAE：固定使用 `CAE訓練\cae_runs\cae_site16_ft_4fold_oof_50ep\final_model\best_cae.pt`，不做厚度分類或 50u/70u 模型切換。
- CAE 訓練：由原 30 張模型初始化，以 16 張乾淨現場樣本 fine-tune 50 epochs；4-fold sample-level OOF 排除 FX17_0003、FX17_0018。
- 均勻度校正：使用同一 run 的 `final_model\site16_oof_calibration.json`；`calibration_pixel_p99 = 4.263018490746617e-05`，由上述 16 張樣本的 OOF residual pixels 合併計算。
- CAE 輸出：30 張均已用 RTX 5080 重跑，位於 `CAE訓練\cae_runs\cae_all_v3\replay_outputs_30`。
- 電阻：使用 `電阻值模型\resistance_model_no_thickness_v3_outputs\resistance_model_no_thickness.npz`。
- 電阻輸入：204 個光譜特徵，不含 `thickness_um` 或厚度路由。
- 電阻驗證：30 張 LOOCV MAE 約 0.129 Ω，RMSE 約 0.159 Ω，R² 約 0.674。
- 樣本邊界：未校正 RAW 逐行校正後，以遲滯狀態機自動判定開始／結束；30 張連續模擬偵測 30/30，最大邊界誤差 0 行。
- Material score：每行先產生 640 個空間訊號，再只統計目前黑白校正響應可靠的 540 欄。排除的欄位在 `white-dark` 分母過小，若納入會放大雜訊並提高空白區誤判風險；正式現場需用新的白校正重新確認有效欄位。
- 串流模擬報表：`stream_detection_outputs\stream_detection_report.json`，分數圖為同資料夾的 `stream_detection_scores.png`。
- 即時推論：`SAMPLE_END` 產生的 `(lines, 640, 204)` float32 cube 直接送入 `LiveAnalysisWorker.submit_cube()`，不需要先寫回 HDR/RAW。
- 背景模型：CAE、PCA 與電阻模型在 worker 啟動時只載入一次；Qt 主執行緒只輪詢結果，不會被 GPU 推論阻塞。
- 完整整合測試：30/30 cube 均完成 GPU 推論，平均每張約 1.90 秒，最慢約 2.23 秒；報表位於 `stream_live_analysis_outputs\stream_detection_report.json`。

介面現在顯示背景 worker 對當次偵測 cube 重新計算的最終模型預測；預先計算值與 LOOCV 數值仍保留在 `replay_manifest.json` 與模型輸出 CSV，僅供結果比對與泛化能力評估。

目前 Qt 資料流為：`未校正錄製行 -> 黑白校正 -> rolling 即時畫面 + 邊界偵測 -> SAMPLE_END cube -> 背景 CAE/電阻推論 -> Qt 結果`。正式版只需把「未校正錄製行」來源替換成 GenTL 相機行。

## 重新產生結果

重訓無厚度電阻模型：

```powershell
python ..\..\電阻值模型\train_resistance_no_thickness.py `
  --features ..\..\電阻值模型\resistance_model_v3_feature_cache\resistance_features.csv `
  --output-dir ..\..\電阻值模型\resistance_model_no_thickness_v3_outputs
```

用整合 CAE 對 30 張重新推論：

```powershell
python ..\..\CAE訓練\infer_cae.py `
  --run-dir ..\..\CAE訓練\cae_runs\cae_all_v3 `
  --data-dir ..\..\CAE訓練\導電材料_RT_v3 `
  --all --include-excluded `
  --output-dir ..\..\CAE訓練\cae_runs\cae_all_v3\replay_outputs_30 `
  --device cuda `
  --calibration-p99 0.0000389240231
```

最後重建回放清單：

```powershell
python build_replay_manifest.py
```

重新執行未校正 RAW 串流偵測：

```powershell
python simulate_stream_detection.py --collect-cubes
python build_replay_manifest.py
```

執行 30 張 `SAMPLE_END -> GPU worker` 完整整合測試：

```powershell
python simulate_stream_detection.py --analyze --analysis-device cuda `
  --output-dir stream_live_analysis_outputs
```

單張比對線上與快取結果：

```powershell
python verify_live_analysis.py --sample 70u_3_RT --device cuda
```

## 目前限制

現有 30 張資料只有 50u 與 70u。這版已移除厚度特徵，仍不能證明模型能正確處理 40u、45u、55u、60u 或 65u；正式測試前需要取得各厚度的有標註樣本做獨立驗證及再訓練。

FX17 worker 已接入 Qt，但現場 GenTL producer 仍須能取得裝置控制權。
worker 嚴格要求 Harvester 將 component 解碼為 `(224, 640)` 的單通道整數
陣列；若仍收到 packed bytes、尺寸不符或裝置不可用，會送出共用的
`StreamMessage(kind="error")`。只有成功建立 image acquirer 後才會依設定
寫入相機節點；所有關閉路徑都會嘗試關快門、停止 acquisition 並釋放裝置。

相機選擇會優先使用設定檔中的序號；若該序號不存在但 GenTL 只偵測到一台
FX17/FX17e，程式會自動連線並在側欄顯示實際型號與序號。若同時偵測到多台
FX17，程式會拒絕猜測並要求指定序號。自動切換至新序號時不會套用舊相機的
推掃 line rate，既有推掃取像參數校正也會標記為失效。

自動裁切 cube 與
原始整張 HDR 相比，30 張均勻度平均絕對差約 0.057 個百分點、最大約
0.792；電阻平均絕對差約 0.0007 Ω。正式定版前仍應使用現場串流裁切規則
重新校準 CAE 的均勻度門檻。
