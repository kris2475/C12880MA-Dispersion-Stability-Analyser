# Multi-Angle Spectral Water Quality Analyser

An open-source optical sensing platform that pairs the **Hamamatsu C12880MA micro-spectrometer** with a multi-port cuvette/immersion housing and machine learning to decouple particle scattering from chemical absorption in complex aqueous solutions.

Traditional ISO 7027 turbidity meters rely on a single near-infrared LED and a 90° photodiode, failing completely when water contains dissolved organic matter (DOM), tannins, algal pigments, or carbon nanomaterials. This project addresses that limitation by capturing full-spectrum (340–850 nm) dual-angle optical signatures to fingerprint water quality components using chemometrics and machine learning.

---

## System Architecture

The hardware architecture moves beyond single-channel nephelometry by capturing light across two optical axes simultaneously:

* **0° Transmission Port:** Measures total optical extinction, combining light absorption from carbon/pigments and forward scattering.
* **45° Forward-Scattering Port:** Isolates Mie scattering to evaluate particle size distribution and concentration while minimizing the inner-filter absorption effects common in dark suspensions.

---

## Sensor Layout & Dark Pixel Reference

The Hamamatsu C12880MA sensor array comprises 288 total channels. The first 87 columns (**P0 through P86**) correspond to a dedicated sequence of physically shielded pixels on the sensor die. 

Rather than relying on an isolated reference point, the manufacturer includes this continuous block of masked elements to serve as a robust optical black reference. Because these elements receive zero light from the diffraction grating, their output captures the sensor's native dark current, reset noise, and electronic baseline offsets, forming the characteristic flat plateau observed at the beginning of the raw data stream prior to the active, light-sensitive pixel region. The firmware uses these values for real-time thermal drift and offset subtraction on every scan.

---

## Hardware Bill of Materials

* **Micro-Spectrometer:** Hamamatsu C12880MA (288-pixel CMOS image sensor with a reflection grating, 340–850 nm range; features 87 optically shielded dark pixels, P0–P86, for real-time baseline noise subtraction)
* **Microcontroller:** ESP32 Dev Module (handling precise clock/trigger timing, ADC attenuation, OLED interface, and SD logging)
* **Light Source:** Broad-spectrum white LED (SunLike 6500K)
* **Housing:** Custom 3D-printed matte black enclosure with integrated 0° and 45° optical channels, light-tight baffling, and optional immersion probe geometry

---

## Optical Physics & Multi-Component Testing

By reading out the sensor array across two angles, the system generates a structured feature tensor per reading to differentiate overlapping chemical and physical signatures:

1. **Graphene & Carbon Nanomaterials (GNPs / GO / Carbon Black):** Exhibit a broad, featureless log-linear extinction slope across the entire 340–850 nm spectrum, heavily dominating transmission data.
2. **Biological Activity (Algae / "Green Slime"):** Introduces distinct absorption dips in the blue (~430–450 nm) and red (~660–680 nm) regions due to chlorophyll-a, turning the optical rig into a fluorometric/absorbance hybrid.
3. **Natural Organic Matter (Tea / DOM / Tannins):** Causes steep UV-blue absorption tails that decay exponentially toward the infrared, simulating real-world humic interference.
4. **Mineral Silt / Milk:** Produces high-intensity, uniform Mie scattering profiles across both 0° and 45° ports without deep absorption features.

---

## Firmware & Data Logging Features

The companion ESP32 firmware features an interactive serial command interface (via Serial Monitor or Tera Term) supporting:
* **Real-Time Auto-Exposure:** Automatically scales integration time (11 ms to 1 s) to keep peak ADC values within target optimal bounds (2500–3500).
* **Periodic Interval Datalogging (`LOG <sec>`):** Automatically saves timestamped spectral frames to an attached SD card at user-defined intervals (e.g., `LOG 15` logs every 15 seconds).
* **Burst Capture (`B`):** Rapidly captures a 10-frame RAM buffer following a pre-flight auto-exposure stabilization phase and writes the batch to the master CSV log.
* **Metadata Tagging (`COMMENT <text>`):** Appends custom text tags directly to log entries for field sample tracking.

---

## Machine Learning & Chemometric Pipeline

The repository includes scripts for processing raw spectral frames and training predictive models:

* **Preprocessing & Normalisation:** Real-time dark-pixel subtraction using channels P0–P86 for thermal drift compensation, pixel-to-wavelength mapping via factory calibration coefficients, and baseline drift correction using clean-water reference blanks.
* **Multi-Output Regression (PLSR / Neural Networks):** Maps the dual-angle spectral tensor to simultaneously predict independent concentrations of suspended solids, carbon nanomaterials, and organic contaminants.
* **Unsupervised Anomaly Detection:** Uses autoencoders or isolation forests trained on baseline water standards to flag sudden pollution events, biological blooms, or structural aggregation shifts via reconstruction error spikes.

