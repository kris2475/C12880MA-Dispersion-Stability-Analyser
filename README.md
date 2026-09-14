# Multi-Angle Spectral Water Quality Analyser

An open-source optical sensing platform that pairs the **Hamamatsu C12880MA micro-spectrometer** with a multi-port cuvette holder and machine learning to decouple particle scattering from chemical absorption in complex aqueous solutions.

Traditional ISO 7027 turbidity meters rely on a single near-infrared LED and a 90° photodiode, failing completely when water contains dissolved organic matter (DOM), tannins, algal pigments, or carbon nanomaterials. This project addresses that limitation by capturing full-spectrum (340–850 nm) dual-angle optical signatures to fingerprint water quality components using chemometrics and machine learning.

---

## System Architecture

The hardware architecture moves beyond single-channel nephelometry by capturing light across two optical axes simultaneously:

* **0° Transmission Port:** Measures total optical extinction (combining light absorption from carbon/pigments and scattering).
* **45° Forward-Scattering Port:** Isolates Mie scattering to evaluate particle size distribution and concentration while minimizing the inner-filter absorption effects common in dark suspensions.

### Hardware Bill of Materials
* **Micro-Spectrometer:** Hamamatsu C12880MA (288-pixel CMOS image sensor with a reflection grating, 340–850 nm range)
* **Light Source:** Broad-spectrum white LED (or multi-wavelength LED matrix) with collimating optics
* **Sample Chamber:** Custom 3D-printed or machined housing with integrated 0° and 45° optical channels and light-tight baffling
* **Microcontroller:** ESP32, Teensy 4.0/4.1, or STM32 (capable of handling precise clock/trigger timing for the C12880MA)

---

## Optical Physics & Multi-Component Testing

By capturing 288 spectral bins across two angles, the system generates a $576$-feature data tensor per reading, enabling the differentiation of complex mixtures:

1. **Graphene & Carbon Nanomaterials (GNPs / GO / Carbon Black):** Exhibit a broad, featureless log-linear extinction slope across the entire 340–850 nm spectrum, heavily dominating transmission data.
2. **Biological Activity (Algae / "Green Slime"):** Introduces distinct absorption dips in the blue (~430–450 nm) and red (~660–680 nm) regions due to chlorophyll-a, turning the turbidity rig into a rudimentary fluorometric/absorbance hybrid.
3. **Natural Organic Matter (Tea / DOM / Tannins):** Causes steep UV-blue absorption tails that decay exponentially toward the infrared, simulating real-world humic interference.
4. **Mineral Silt / Milk:** Produces high-intensity, uniform Mie scattering profiles across both 0° and 45° ports without deep absorption features.

---

## Machine Learning & Chemometric Pipeline

The repository includes scripts for processing raw spectral frames and training predictive models:

* **Preprocessing & Normalization:** Dark-frame subtraction, pixel-to-wavelength mapping via factory coefficients, and baseline drift correction using clean-water reference blanks.
* **Multi-Output Regression (PLSR / Neural Networks):** Maps the dual-angle spectral tensor to simultaneously predict independent concentrations of suspended solids, carbon nanomaterials, and organic contaminants.
* **Unsupervised Anomaly Detection:** Utilizes autoencoders or isolation forests trained on baseline water standards to flag sudden pollution events, biological blooms, or structural aggregation shifts via reconstruction error spikes.

---

