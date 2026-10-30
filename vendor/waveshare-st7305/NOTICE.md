# Waveshare ST7305 attribution

Reference: [waveshareteam/ESP32-S3-RLCD-4.2](https://github.com/waveshareteam/ESP32-S3-RLCD-4.2/tree/d8bf7813d3b53251577ff03c03596ad6ad3acae0), fixed commit `d8bf7813d3b53251577ff03c03596ad6ad3acae0`. Original project is licensed under Apache-2.0; the exact LICENSE and two reference source files accompany this notice.

Adapted portions: ST7305 command/data initialization and the 300×400 portrait pixel mapping. The product implementation lives in `integration/product/platform/ST7305_Waveshare.*` and `st7305_waveshare_*.hpp`. Changes replace vendor UI/task/DMA scaffolding with Draftor’s synchronous framebuffer/shadow contract, bounded row windows, aligned source staging and black/white focus affordances. No vendor example application is built into the product. The 10 MHz bus is taken from the LVGL example; initialization agrees with the U8g2 reference. Original Draftor licensing is unchanged.
