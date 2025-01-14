<!-- Improved compatibility of back to top link: See: https://github.com/othneildrew/Best-README-Template/pull/73 -->
<a name="readme-top"></a>
<!--
*** Thanks for checking out the Best-README-Template. If you have a suggestion
*** that would make this better, please fork the repo and create a pull request
*** or simply open an issue with the tag "enhancement".
*** Don't forget to give the project a star!
*** Thanks again! Now go create something AMAZING! :D
-->



<!-- PROJECT SHIELDS -->
<!--
*** I'm using markdown "reference style" links for readability.
*** Reference links are enclosed in brackets [ ] instead of parentheses ( ).
*** See the bottom of this document for the declaration of the reference variables
*** for contributors-url, forks-url, etc. This is an optional, concise syntax you may use.
*** https://www.markdownguide.org/basic-syntax/#reference-style-links
-->
[![Contributors][contributors-shield]][contributors-url]
[![Forks][forks-shield]][forks-url]
[![Stargazers][stars-shield]][stars-url]
[![Issues][issues-shield]][issues-url]
[![GPL3.0 License][license-shield]][license-url]



<!-- PROJECT LOGO -->
<div align="center">

<h3 align="center">VWCDC - ESP32</h3>

  <p align="center">
    CD Changer Emualtor For VW Radios 
    <br />
    <a href="https://github.com/NullString1/VWCDC"><strong>Explore the docs »</strong></a>
    <br />
    <br />
    <a href="https://github.com/NullString1/VWCDC/issues">Report Bug</a>
    ·
    <a href="https://github.com/NullString1/VWCDC/issues">Request Feature</a>
  </p>
</div>



<!-- TABLE OF CONTENTS -->
### Table of Contents
<ol>
  <li>
    <a href="#about-the-project">About The Project</a>
  </li>
  <li>
    <a href="#getting-started">Getting Started</a>
    <ul>
      <li><a href="#using-platformio">Using platformio</a></li>
    </ul>
  </li>
  <li><a href="#usage">Usage</a></li>
  <li><a href="#car-radio-models">Car Radio Models</li>
  <li><a href="#roadmap">Roadmap</a></li>
  <li><a href="#contributing">Contributing</a></li>
  <li><a href="#license">License</a></li>
  <li><a href="#contact">Contact</a></li>
  <li><a href="#related-projects-for-different-architectures">Related Projects For Different Architectures</a></li>
  <li><a href="#useful-information">Useful information</a></li>
    
</ol>



<!-- ABOUT THE PROJECT -->
## About The Project
This project emulates the CD Changer in older VW / Audi / Skoda / Siat cars to allow the radio to accept AUX input from phone or bluetooth module. 

Port of [VAG CDC Faker by shyd](https://schuett.io/2013/09/avr-raspberry-pi-vw-beta-vag-cdc-faker/) for the ESP32
<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- GETTING STARTED -->
## Getting Started

### Using platformio

1. Install vscode
2. Install platformio
3. Install `Espressif 32` platform
4. [Download](https://github.com/NullString1/VWCDC/archive/refs/heads/master.zip) and extract project
5. Connect ESP32
6. Run `Upload` task in platformio with ESP32 in download mode

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- USAGE EXAMPLES -->
## Usage

Connect ESP32 to radio following the wiring diagram

| Name             | ESP32 Pin | Radio Pin | Purpose                  |
|------------------|-----------|-----------|--------------------------|
| SCLK             | 18        | CDC CLOCK | SPI Clock Signal 62.5MHz |
| MOSI             | 23        | DATA IN   | Data from ESP to radio   |
| MISO / Radio Out | 17        | DATA OUT  | Data from radio to ESP   |

<p align="right">(<a href="#readme-top">back to top</a>)</p>

## Car Radio Models

<details>
  <summary>Tested</summary>
  
  - RCD200 mp3 (VW POLO 2008)
  - Seat Audio System Aura CD2 (Seat Toledo II (2004)) - Credit to [@Ammar1605](https://github.com/Ammar1605)
</details>
<details>
  <summary>Unverified</summary>
  Untested with this port but working with related projects using same protocol (https://github.com/tomaskovacik/vwcdpic)

- Audi concert 1(blaupunkt)
- Audi concert 1(philips)
- Audi concert 2
- Chorus1(blaupunkt)
- Symohony I
- VW blaupunkt RadioNavigationSystem MCD
- VW Passat Blaupunkt Gamma (similar to Gamma V)
- Audi Symphony II BOSE
- Volkswagen
  - New Beetle
  - Touareg
  - Passat B3/B4 (1993+)
  - EOS 3.2 (2008)
  - Single DIN Monsoon
  - Single DIN Monsoon with In-Dash CD Player
  - Double DIN Monsoon
  - Multi-Function Display (MFD)
  - Gamma IV
  - Gamma V
  - RCD300
  - Blaupunkt R100
  - RNS300
  - RN S2 DVD
- Seat
  - Leon
  - Ibiza
- Skoda
  - Symphony
- Audi
  - Audi Chorus II
</details>


<!-- ROADMAP -->
## Roadmap

- [ ] Media control via bluetooth
  - [ ] CDs mapped to playlists

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- CONTRIBUTING -->
## Contributing

Contributions are what make the open source community such an amazing place to learn, inspire, and create. Any contributions you make are **greatly appreciated**.

If you have a suggestion that would make this better, please fork the repo and create a pull request. You can also simply open an issue with the tag "enhancement".
Don't forget to give the project a star! Thanks again!

1. Fork the Project
2. Create your Feature Branch (`git checkout -b feature/AmazingFeature`)
3. Commit your Changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the Branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

<p align="right">(<a href="#readme-top">back to top</a>)</p>



<!-- LICENSE -->
## License

Distributed under the GPL-3.0 license. See `LICENSE` for more information.

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- CONTACT -->
## Contact

Daniel Kern (NullString1) - [@nullstring1_](https://twitter.com/nullstring1_) - daniel@nullstring.one

Website: [https://nullstring.one](https://nullstring.one)
Project Link: [https://github.com/NullString1/VWCDC](https://github.com/NullString1/VWCDC)

<p align="right">(<a href="#readme-top">back to top</a>)</p>



## Related Projects For Different Architectures
- AVR https://github.com/tomaskovacik/vwcdavr
- AVR https://schuett.io/2013/09/avr-raspberry-pi-vw-beta-vag-cdc-faker/
- PIC https://github.com/tomaskovacik/vwcdpic
- STM32 https://github.com/tomaskovacik/vwcdstm32

## Useful information
- http://kuni.bplaced.net/_Homepage/CdcEmu/CdcEmu_Versions_e.html
- https://web.archive.org/web/20220810224519/https://martinsuniverse.de/projekte/cdc_protokoll/cdc_protokoll.html
- https://q1.se/cdcemu/details.php (Different protocol but related)
<p align="right">(<a href="#readme-top">back to top</a>)</p>



<!-- MARKDOWN LINKS & IMAGES -->
<!-- https://www.markdownguide.org/basic-syntax/#reference-style-links -->
[contributors-shield]: https://img.shields.io/github/contributors/NullString1/VWCDC.svg?style=for-the-badge
[contributors-url]: https://github.com/NullString1/VWCDC/graphs/contributors
[forks-shield]: https://img.shields.io/github/forks/NullString1/VWCDC.svg?style=for-the-badge
[forks-url]: https://github.com/NullString1/VWCDC/network/members
[stars-shield]: https://img.shields.io/github/stars/NullString1/VWCDC.svg?style=for-the-badge
[stars-url]: https://github.com/NullString1/VWCDC/stargazers
[issues-shield]: https://img.shields.io/github/issues/NullString1/VWCDC.svg?style=for-the-badge
[issues-url]: https://github.com/NullString1/VWCDC/issues
[license-shield]: https://img.shields.io/github/license/NullString1/VWCDC.svg?style=for-the-badge
[license-url]: https://github.com/NullString1/VWCDC/blob/master/LICENSE
