# Multiwavelength Dispersion Analyser

A low-cost, highly portable optical sensing platform designed for field and lab use, pairing the **Hamamatsu C12880MA micro-spectrometer** with its **onboard PCB LED illumination source** to track stability, flocculation, agglomeration, and sedimentation dynamics in complex liquid dispersions.

Traditional stability analysers rely on single-wavelength light sources and a single photodiode, capturing only a blunt aggregate transmission number. This project evolves that proven industrial concept into a full-spectrum (340 - 850 nm) optical tool. By monitoring every wavelength simultaneously, the system can decouple overall scattering mass from time-dependent particle size shifts, micro-bubble relaxation phases, and differential sedimentation.


<img src="C12880MA_Dispersion_Analyser/IMG20260923084638.jpg" alt="Multiwavelength Dispersion Analyser" width="500">

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

* **Micro-Spectrometer & Light Source:** Hamamatsu C12880MA micro-spectrometer module utilising its **onboard PCB LED** as the illumination source (288-pixel CMOS image sensor with a reflection grating, 340 - 850 nm range; features 87 optically shielded dark pixels, P0 - P86, for real-time baseline noise subtraction).
* **Microcontroller:** ESP32 Dev Module (handling precise clock timing, ADC attenuation, OLED interface, and SD card logging).
* **Housing:** Custom 3D-printed matte black, lightweight portable enclosure optimised for standard optical vials/cuvettes and secure alignment with the spectrometer PCB.

---

## Dispersion Physics & Kinetic Analysis Pipeline

By capturing full-spectrum temporal data (e.g., minute-by-minute logs of shaken suspensions like turmeric or starch in water), the companion Python and firmware tools extract rich kinetic metrics underpinned by fundamental optical scattering and fluid dynamics principles:

1. **Sedimentation Tracking (Total Scattering Mass & Stokes' Law):** 
   * Monitors the total integrated intensity across all active wavelengths to evaluate overall suspended particle mass in real time.
   * Captures initial preparation artefacts, such as micro-bubble clearance and de-aeration phases immediately following sample agitation.
   * Reflects bulk particle concentration changes governed by gravitational settling dynamics (approximate Stokes' settling), where particle size, mass density differentials, and fluid viscosity dictate how matter migrates relative to the optical path.

2. **Agglomeration & Flocculation Tracking (Mie vs. Rayleigh Scattering & Spectral Slope Ratios):** 
   * Computes multiwavelength intensity ratios (e.g., $650\text{nm} / 450\text{nm}$) to detect continuous changes in spectral tilt and slope as particles interact and evolve.
   * **Rayleigh Regime ($d \ll \lambda$):** Applies when scattering particles are significantly smaller than the incident wavelength (typically $d < \lambda / 10$). Here, scattering efficiency scales steeply with the inverse fourth power of wavelength ($\lambda^{-4}$), causing much stronger scattering at shorter wavelengths (blue/UV) than longer wavelengths (red/NIR).
   * **Mie Regime ($d \approx \lambda$):** Applies when particle diameters are comparable to the wavelength of light, which is characteristic of practical suspensions, emulsions, and colloidal dispersions (such as starch or turmeric granules). In this regime, the wavelength dependence becomes much weaker and non-monotonic, scaling inversely with $\lambda$ to a lower power or exhibiting complex resonance patterns.
   * **Tracking Agglomeration:** By measuring how the full spectrum shifts across these optical regimes, the system isolates differential sedimentation and flocculation, revealing how heavier or coarser aggregates drop out rapidly while finer fractions remain suspended or form loose structural clusters that alter the wavelength-dependent scattering profile.

3. **Peak Wavelength & Structural Shape Shifts:** 
   * Identifies discrete shifts, broadening, or fluctuations in maximum scattering peaks over extended time series.
   * Decouples bulk chemical identity and baseline intensity changes from physical structural modifications, confirming whether the primary scattering centre remains stable while localised clustering, swelling, or spatial redistribution occurs within the dispersion.

---

## Firmware & Field-Ready Data Logging

The companion ESP32 firmware features an interactive serial interface supporting deployment in the field:
* **Real-Time Auto-Exposure:** Automatically scales integration time (11 ms to 1 s) to keep peak ADC values within optimal target bounds.
* **Periodic Interval Datalogging (`LOG <sec>`):** Automatically saves timestamped spectral frames to an attached SD card at user-defined intervals (ideal for tracking long-term stability curves over 30 to 60+ minutes).
* **Burst Capture (`B`):** Rapidly captures a RAM buffer following a pre-flight auto-exposure stabilisation phase.
* **Metadata Tagging (`COMMENT <text>`):** Appends custom text tags directly to log entries for field sample tracking.

## Experimental Results

To validate the multiwavelength pipeline under real-world conditions, a dynamic stability test was performed using a turmeric-in-water dispersion over a 36-minute time series. Following manual agitation, the sample was logged continuously to capture both transient mixing artifacts and long-term suspension relaxation.

### 1. Raw Spectral Evolution: Waterfall & Spectrogram View
The initial optical behavior of the dispersion is captured across the raw spectral response. 

<img src="C12880MA_Dispersion_Analyser/Spectrogram.png" alt="Waterfall plot of raw turmeric spectra" width="700"> -->

* **What we are seeing:** The raw waterfall/overlay plot above demonstrates the complete spectral signature captured by the Hamamatsu C12880MA spectrometer, ranging from the UV-visible cutoff through the characteristic curcuminoid absorption and scattering bands (peaking around 560 – 590 nm). 
* **Connection to the Pipeline:** This corresponds directly to the raw data input stage of our pipeline. Unlike a single-wavelength sensor that only logs a single scalar value, this full spectral sweep captures the entire optical envelope at every 60-second time step. The dense vertical lines visible in early iterations highlight how the hardware pixel resolution responds across the spectrum before baseline dark-correction is applied.

---

### 2. Kinetic Breakdown & Multi-Parameter Analysis
To understand the physical phenomena occurring inside the vial, the raw spectra were processed through my multiwavelength analytical framework, yielding three distinct diagnostic time profiles.

<img src="C12880MA_Dispersion_Analyser/plots.png" alt="Three-panel kinetic analysis of turmeric dispersion" width="700">



The dispersion stability analysis provides insights into how the sample's spectral properties change over time, indicating potential physical processes like sedimentation, agglomeration, or flocculation.

Important Context: The dispersion was shaken before logging, which might influence initial spectral readings due to temporary air bubbles or increased homogeneity. After the logging run, sedimentation of turmeric particles was observed at the bottom of the vial.

Automated Dispersion Stability Report Breakdown:

Sedimentation Rate: The report indicates a sedimentation rate of 2.280% change per minute. The status "Suspension is stable against heavy sedimentation." suggests that while there is some change, it's not severe enough to be classified as significant particle dropout based on the defined threshold (-0.5%). The first plot, "1. Sedimentation Tracking (Total Scattering Mass)", visually represents this trend. A relatively flat line close to 100% (initial intensity) confirms stability against rapid settling.

Agglomeration Slope: The agglomeration slope is reported as -0.00606 units/min, triggering an alert: "Spectral slope shifting, indicating possible particle growth/flocculation." This suggests that the ratio of intensities at 650nm to 450nm is changing, which can be an indicator of particles either growing in size or aggregating together. The second plot, "2. Agglomeration / Flocculation Tracking (Spectral Slope)", illustrates this spectral ratio over time. A noticeable trend (either increasing or decreasing) in this plot supports the alert, implying changes in particle size distribution.

Peak Shift Tracking: The third plot, "3. Peak Shift Tracking", shows how the wavelength of the maximum intensity changes over time. While the automated report didn't explicitly comment on this metric, this plot helps visualise any shifts in the characteristic absorption or scattering peak of the sample. Significant shifts could indicate chemical reactions or substantial changes in particle morphology/size.

* **Panel 1: Sedimentation Tracking (Total Scattering Mass)**
  * **What it shows:** The normalised total integrated intensity starts at a baseline of 100 % and climbs sharply over the first 20 minutes before stabilising near 180 - 190 %. 
  * **Physics Connection:** This unmasks the **micro-bubble clearance phase**. Vigorous manual shaking introduces micro-bubbles that severely scatter and block light, artificially depressing the initial transmission signal. As bubbles migrate to the surface and dissipate, the optical path clears, revealing the true underlying scattering behaviour of the suspension mass.
* **Panel 2: Agglomeration / Flocculation Tracking (Spectral Slope)**
  * **What it shows:** The intensity ratio between 650 nm and 450 nm plummets dramatically from an initial extreme value of ~17.5 down to ~3 within the first 5 minutes.
  * **Physics Connection:** This tracks the breakdown and structural relaxation of large, forced agglomerates created during manual mixing. The high initial slope indicates a heavily skewed scattering profile dominated by large clusters. As these clusters settle or break apart into a more uniform colloidal distribution, the spectral tilt flattens into a stable steady state.
* **Panel 3: Peak Shift Tracking**
  * **What it shows:** The apex wavelength of the primary dispersion band oscillates in a regular quantisation pattern between ~587.5 nm and ~594.5 nm, with transient excursions up to 605 nm.
  * **Physics Connection:** This demonstrates the limits and behaviour of **pixel-resolution quantisation** on discrete spectrometer arrays. As the true optical peak centre shifts subtly, the algorithm snaps between adjacent physical pixel columns. The larger spikes (e.g., at minute 22) capture genuine, momentary bulk shifts where a broader particle cluster migration transiently altered the spectral envelope's center of mass.
 
### 2. Sedimentation vs. Spectral Ratio

The strong negative correlation (-0.9834) observed between normalized total intensity (sedimentation) and the spectral ratio (agglemoration/flocculation) can be clearly visualized using a scatter plot. This plot helps to confirm the inverse relationship and understand its linearity.

<img src="C12880MA_Dispersion_Analyser/Sedimentation_vs_Spectral_Ratio.png" alt="Sedimentation vs. Spectral Ratio" width="700">

Correlation Analysis: Sedimentation vs. Spectral Ratio
Pearson Correlation Coefficient: The calculated Pearson Correlation between Sedimentation (Normalised Total Intensity) and Spectral Ratio (650nm/450nm) is -0.9834.

Significance: This indicates a very strong negative correlation. A correlation coefficient close to -1 suggests that as one variable increases, the other decreases proportionally. In this context:

As the Normalised Total Intensity (Sedimentation) increases (meaning less sedimentation or even an increase in apparent scattering due to other factors), the Spectral Ratio (Agglomeration/Flocculation) decreases.
This strong inverse relationship suggests that the processes influencing the overall scattering intensity are tightly coupled with those altering the spectral slope. For instance, if particles are settling, the total intensity might decrease, and if they are also agglomerating, the spectral ratio might change in a corresponding manner. The observed strong negative correlation implies that the spectral changes related to particle size (reflected in the spectral ratio) are consistently happening in tandem with changes in the total dispersed mass or scattering strength (reflected in the normalised total intensity).

