# Multiwavelength Dispersion Analyser

A low-cost, highly portable optical sensing platform designed for field and lab use, pairing the **Hamamatsu C12880MA micro-spectrometer** with its **onboard PCB LED illumination source** to track stability, flocculation, agglomeration, and sedimentation dynamics in complex liquid dispersions.

Traditional stability analysers rely on single-wavelength light sources and a single photodiode, capturing only a blunt aggregate transmission number. This project evolves that proven industrial concept into a full-spectrum (340–850 nm) optical tool. By listening to every wavelength simultaneously, the system can decouple overall scattering mass from time-dependent particle size shifts, micro-bubble relaxation phases, and differential sedimentation.

---

## System Architecture & Field Portability

Engineered for rapid deployment outside the traditional laboratory, the device combines low-power microcontrollers with precision optical hardware:

* **0° Transmission & Extinction Axis:** The primary operational path, tracking total optical density, scattering, and initial de-aeration/bubble-clearing behaviour immediately after sample agitation.
* **Multi-Angle Optical Probing:** Configurable housing geometry designed to capture multiwavelength extinction and scattering profiles across the entire visible-to-near-infrared spectrum.

---

## Sensor Layout & Dark Pixel Reference

The Hamamatsu C12880MA sensor array comprises 288 total channels. The first 87 columns (**P0 through P86**) correspond to a dedicated sequence of physically shielded pixels on the sensor die. 

Rather than relying on an isolated external reference, these masked elements capture the sensor's native dark current, reset noise, and electronic baseline offsets in real time. The firmware uses this continuous block for thermal drift and offset subtraction on every individual scan, ensuring high repeatability in field conditions.

---

## Hardware Bill of Materials

* **Micro-Spectrometer & Light Source:** Hamamatsu C12880MA micro-spectrometer module utilising its **onboard PCB LED** as the illumination source (288-pixel CMOS image sensor with a reflection grating, 340–850 nm range; features 87 optically shielded dark pixels, P0–P86, for real-time baseline noise subtraction).
* **Microcontroller:** ESP32 Dev Module (handling precise clock timing, ADC attenuation, OLED interface, and SD card logging).
* **Housing:** Custom 3D-printed matte black, lightweight portable enclosure optimised for standard optical vials/cuvettes and secure alignment with the spectrometer PCB.

---

## Dispersion Physics & Kinetic Analysis Pipeline

By capturing full-spectrum temporal data (e.g., minute-by-minute logs of shaken suspensions like turmeric or starch in water), the companion Python and firmware tools extract three core kinetic metrics:

1. **Sedimentation Tracking (Total Scattering Mass):** Monitors the total integrated intensity across all active wavelengths to evaluate overall particle suspension mass and capture preparation artefacts, such as micro-bubble clearance immediately following sample shaking.
2. **Agglomeration & Flocculation Tracking (Spectral Slope Ratios):** Computes intensity ratios (e.g., 650 nm / 450 nm) to detect changes in the spectral tilt. This isolates differential sedimentation, where heavier coarser aggregates drop out rapidly while finer micro-particles remain suspended.
3. **Peak Wavelength & Shape Shifts:** Identifies shifts in maximum scattering peaks to track structural changes or particle growth within the dispersion over extended time series.

---

## Firmware & Field-Ready Data Logging

The companion ESP32 firmware features an interactive serial interface supporting deployment in the field:
* **Real-Time Auto-Exposure:** Automatically scales integration time (11 ms to 1 s) to keep peak ADC values within optimal target bounds.
* **Periodic Interval Datalogging (`LOG <sec>`):** Automatically saves timestamped spectral frames to an attached SD card at user-defined intervals (ideal for tracking long-term stability curves over 30 to 60+ minutes).
* **Burst Capture (`B`):** Rapidly captures a RAM buffer following a pre-flight auto-exposure stabilisation phase.
* **Metadata Tagging (`COMMENT <text>`):** Appends custom text tags directly to log entries for field sample tracking.

