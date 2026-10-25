# 雙點溫濕度監測與結露預警系統

**Dual-Point Temperature/Humidity Monitor with Dew-Point Warning**

用兩顆 DHT11 監測電控箱內外的溫濕度，ESP32-S3 即時計算露點，在接近結露條件時發出預警，資料經 MQTT 上傳 ThingSpeak 並以 MATLAB 視覺化。

`ESP32-S3` `DHT11` `MQTT` `ThingSpeak` `MATLAB` `Arduino`

---

## 動機

CO₂、乾冰等低溫設備附近的電控箱，在潮濕季節容易結露，箱內的 PLC、變頻器與繼電器受潮後可能故障停機。現場通常要等異常發生才發現。

本專題用低成本感測器與物聯網，在**結露發生前**提出預警。

## 系統架構

![系統架構](docs/architecture.svg)

| 元件 | 用途 |
| --- | --- |
| ESP32-S3 開發板 | 讀取感測器、計算、Wi-Fi 與 MQTT 通訊 |
| DHT11 × 2 | 監測點（A）與參考環境（B）的溫濕度 |
| ThingSpeak（免費帳號） | MQTT Broker、資料儲存、MATLAB 視覺化 |

## 功能

- **露點與絕對濕度計算**（Magnus 公式）
- **結露預警**：監測點溫度 − 露點 < 門檻（預設 3°C）時觸發
- **感測器故障偵測**：讀值為 NaN 時判定故障，只上傳有效數值，另一顆照常運作
- **MQTT 上傳**：每 20 秒發佈一次，8 個欄位 + 狀態碼
- **雲端視覺化**：溫度、露點、預警線疊圖，預警時間點自動標記

## 原理

露點 T<sub>d</sub>（T：°C，RH：%，a = 17.62，b = 243.12）：

$$\gamma = \frac{aT}{b+T} + \ln\left(\frac{RH}{100}\right), \qquad T_d = \frac{b\gamma}{a-\gamma}$$

絕對濕度（g/m³）：相對濕度會隨溫度改變，比較兩個不同溫度的位置時，要用絕對濕度才能反映真正的水氣量。

$$AH = \frac{6.112 \cdot e^{\frac{17.67T}{T+243.5}} \cdot RH \cdot 2.1674}{273.15+T}$$

預警條件：

$$T_A - T_{d,A} < \Delta T_{margin}$$

| 狀態碼 | 意義 |
| --- | --- |
| 0 | 正常 |
| 1 | 結露預警 |
| 2 | 感測器 A 故障 |
| 3 | 感測器 B 故障 |
| 4 | 兩顆皆故障 |

## 實驗結果

靜置時溫度約 28.2°C、露點約 22.9°C，差距約 5.3°C。對感測器 A 呼氣後，露點迅速升至約 24.6°C，再隨水氣散去逐步回落。

![呼氣測試](docs/breath_test_chart.png)

| 階段 | A 溫度 | A 露點 | 溫度 − 露點 |
| --- | --- | --- | --- |
| 靜置 | ≈ 28.2°C | ≈ 22.9°C | ≈ 5.3°C |
| 呼氣峰值 | ≈ 28.6°C | ≈ 24.6°C | ≈ 4.0°C |
| 回穩 | ≈ 28.5°C | ≈ 23.2°C | ≈ 5.3°C |

呼氣時差距約 4.0°C，未低於預設門檻 3°C。展示時將門檻調為 4.5°C（介於 4.0 與 5.3 之間），即可呼氣觸發、靜置不誤報；實際電控箱建議維持 2～3°C。

## 硬體接線

| DHT11 | 感測器 A | 感測器 B |
| --- | --- | --- |
| VCC | 3.3V | 3.3V |
| DATA | GPIO 4 | GPIO 17 |
| GND | GND | GND |

- 以 3.3V 供電，避免 DATA 被上拉到 5V 損傷 ESP32 GPIO。
- 裸感測器需在 DATA 與 VCC 之間加 10kΩ 上拉電阻；模組版已內建。

## 快速開始

1. Arduino IDE 2 安裝 **esp32 by Espressif Systems**，開發板選 **ESP32S3 Dev Module**，`USB CDC On Boot` 設為 **Enabled**（使用原生 USB 孔時）。
2. 函式庫管理員安裝 **PubSubClient**（Nick O'Leary）與 **DHT sensor library**（Adafruit）。
3. ThingSpeak 建立 Channel（8 個欄位，名稱用英文：`A_Temp`、`A_Humidity`、`B_Temp`、`B_Humidity`、`A_DewPoint`、`A_AbsHumidity`、`DeltaT`、`Status`），再到 **Devices → MQTT** 新增裝置並授權該 Channel 的 Publish。
4. 複製 `dew_point_monitor/secrets.example.h` 為 `secrets.h`，填入 Wi-Fi 與 MQTT 帳密。
5. 上傳程式，序列埠監控視窗設 115200 baud。
6. 在 ThingSpeak 新增 MATLAB Visualization，貼上 `matlab/temp_vs_dewpoint.m`。

## 專案結構

```
dew-point-monitor/
├── dew_point_monitor/
│   ├── dew_point_monitor.ino   # ESP32-S3 韌體
│   └── secrets.example.h       # 帳密範本（secrets.h 不上傳）
├── matlab/
│   └── temp_vs_dewpoint.m      # ThingSpeak 視覺化
└── docs/
    ├── architecture.svg
    └── breath_test_chart.png
```

## 開發問題與解決

| 問題 | 原因 | 解決 |
| --- | --- | --- |
| 序列埠只顯示 `ESP-ROM`，沒有程式輸出 | ESP32-S3 的 `USB CDC On Boot` 未啟用，輸出走 UART0 腳位 | 啟用 CDC，並在 `setup()` 加延遲等 USB 連線 |
| 上傳失敗：無法開啟 COM 埠 | 監控視窗佔用埠號；原生 USB 重置後埠號改變 | 關閉監控視窗、重選埠號，必要時用 BOOT + RST 進下載模式 |
| MATLAB：`Duplicate table variable name` | 中文欄位名稱轉成變數時被刪除，造成重名 | 欄位名稱改為英文 |
| 圖表時間差 8 小時 | `thingSpeakRead` 回傳 UTC 時間 | 時間加 8 小時 |

## 限制與未來展望

- DHT11 精度約 ±2°C / ±5%RH，範圍 0～50°C；可升級 SHT31 / SHT40，低溫段改用 PT100。
- 加入 **RS485 Modbus RTU**，讓 PLC / HMI 直接讀取。
- 以繼電器連動箱內**加熱器或風扇**主動除濕。
- 預警推播至 **LINE Messaging API**。
- 累積長期資料，以趨勢分析或機器學習**預測結露發生時間**。
