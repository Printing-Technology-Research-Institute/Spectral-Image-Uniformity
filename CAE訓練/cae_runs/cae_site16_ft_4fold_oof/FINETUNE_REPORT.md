# FX17 現場 16 張樣本 fine-tune 結果

## 設定

- 基礎模型：`cae_all_v3`
- PCA：沿用基礎模型的 8-component PCA，不重新擬合
- 訓練樣本：FX17_0001～FX17_0017，排除疑似污染的 FX17_0003 與 FX17_0018，共 16 張
- 遮罩：SAM 初始遮罩後只保留最大 8-connected component
- 驗證：4-fold、sample-level OOF；每 fold 12 張訓練、4 張驗證，同一張樣本的 patch 不跨 train/validation
- patch：32×32，訓練 stride 8，共 25,435 patches
- fine-tune：從同一份 `cae_all_v3/best_cae.pt` 初始化全部 CAE 權重；learning rate 1e-4、batch size 256、weight decay 1e-5、30 epochs

## 4-fold OOF 結果

- OOF 樣本數：16
- OOF 遮罩內像素數：677,454
- 原模型 pooled mean residual：3.39584e-05
- fine-tune OOF pooled mean residual：1.96248e-05
- pooled mean residual 改善：42.21%
- 每張樣本改善範圍：40.00%～43.69%
- 新 calibration_pixel_p95：3.08368e-05
- 新 calibration_pixel_p99：4.35781e-05
- 四個 fold 的最佳 epoch：30、30、30、30

新 p99 比舊的 3.89240e-05 稍高，是因為新值由完全未參與該 fold 訓練的 OOF 像素估計，較能反映未見現場樣本的尾端殘差。原模型若直接套用相同 16 張現場 OOF 像素，其 p99 為 1.47234e-04。

## 最終模型

- 使用全部 16 張乾淨現場樣本訓練 30 epochs
- final train loss：0.0543183
- 權重：`final_model/best_cae.pt`
- PCA：`final_model/pca_model.npz`
- OOF 校正：`final_model/site16_oof_calibration.json`
- 18 張逐張評估：`final_model/evaluation_metrics.csv`

0003 與 0018 未參與訓練或校正，只作 challenge 檢查。0003 的最終殘差仍高於乾淨樣本（2.48380e-05），支持其資料受到污染或內容異常的判斷；0018 在只保留最大連通元件後為 1.98134e-05，但這不代表其污染問題已被修復。

## 注意

這次只產生新模型與評估結果，未覆寫 `cae_all_v3`，也未切換現場 UI 的預設模型。正式切換時，CAE run path 與 calibration JSON 必須一起更新。
