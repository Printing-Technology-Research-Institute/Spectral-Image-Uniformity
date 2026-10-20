# 推掃取像參數校正（形變校正）

「開始校正」使用既有的 `StreamMessage` 與 `StreamWorker` 資料流，不建立第二套 Qt 串流介面。開始後 detector 保持連續顯示，樣本邊界偵測器收集一個完整通過的圓形標準件；該 cube 只用於橢圓擬合，不送入 GPU 模型。

## 操作流程

1. 先讓輸送帶達到穩定速度。
2. 按下「開始校正」，再放入平整、完整且不碰視野邊界的圓形標準件。
3. 程式先讀取目前 FX17 line rate；自由運轉模式下使用 `AcquisitionFrameRateMax` 作為目前值。
4. 標準件離開後，程式計算 `H/W` 與 `line_rate_multiplier = W/H`。
5. 目標值為 `new_line_rate = current_line_rate × W/H`。程式啟用 `EnAcquisitionFrameRate`、寫入 `AcquisitionFrameRate`，再讀回確認。
6. 成功結果寫入 `settings.example.json` 的 `geometry_calibration`。rolling 偽色和 GPU cube 不做事後拉伸，後續資料直接以新取像參數取得。
7. 本版依需求不進行第二次標準件驗證，設定中的 `verified_second_pass` 固定為 `false`。

圓形品質門檻會拒絕碰邊、低對比、輪廓破碎或明顯偏斜的候選物。相機和 recorded 來源共用同一套模式切換；校正或停止分析時只關閉樣本分析，detector preview 不會停止。

## 儲存欄位

成功後會在設定檔保存：

- `previous_line_rate_hz`
- `requested_line_rate_hz`
- `applied_line_rate_hz`
- `line_rate_multiplier`
- `height_width_ratio`
- `width_px`、`height_lines`
- `ellipse_fill`、`solidity`、`relative_contrast`
- `sample_id`、`exposure_ms`、`updated_at`
- `verified_second_pass: false`

介面不再要求輸入或保存輸送帶速度與工作距離。曝光時間變更時，程式會讓既有校正失效並保留失效原因；如果實際輸送帶速度、鏡頭、工作距離或 ROI/binning 改變，使用者也應主動重新執行校正。
