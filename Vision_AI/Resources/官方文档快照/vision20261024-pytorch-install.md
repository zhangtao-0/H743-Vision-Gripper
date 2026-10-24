Search

[Close Search](https://pytorch.org/get-started/locally/#)

# Get Started

Select preferences and run the command to install PyTorch locally, or get started quickly with one of the supported cloud platforms.

[Menu](https://pytorch.org/get-started/locally/#)

- [Start Locally](https://pytorch.org/get-started/locally/)
- [Additional Platforms](https://pytorch.org/get-started/additional-platforms/)
- [PyTorch 2.x](https://pytorch.org/get-started/pytorch-2-x/)
- [Start via Cloud Partners](https://pytorch.org/get-started/cloud-partners/)
- [Previous PyTorch Versions](https://pytorch.org/get-started/previous-versions/)
- [PyTorch for Edge](https://pytorch.org/get-started/executorch/)

Shortcuts

- [Prerequisites](https://pytorch.org/get-started/locally/#mac-prerequisites)
- [macOS Version](https://pytorch.org/get-started/locally/#macos-version)
- [Python](https://pytorch.org/get-started/locally/#mac-python)
- [Package Manager](https://pytorch.org/get-started/locally/#mac-package-manager)
- [Installation](https://pytorch.org/get-started/locally/#mac-installation)
- [pip](https://pytorch.org/get-started/locally/#mac-pip)
- [Verification](https://pytorch.org/get-started/locally/#mac-verification)
- [Building from source](https://pytorch.org/get-started/locally/#mac-from-source)
- [Prerequisites](https://pytorch.org/get-started/locally/#mac-prerequisites-2)
- [Prerequisites](https://pytorch.org/get-started/locally/#linux-prerequisites)
- [Supported Linux Distributions](https://pytorch.org/get-started/locally/#supported-linux-distributions)
- [Python](https://pytorch.org/get-started/locally/#linux-python)
- [Package Manager](https://pytorch.org/get-started/locally/#linux-package-manager)
- [Installation](https://pytorch.org/get-started/locally/#linux-installation)
- [pip](https://pytorch.org/get-started/locally/#linux-pip)
- [Verification](https://pytorch.org/get-started/locally/#linux-verification)
- [Building from source](https://pytorch.org/get-started/locally/#linux-from-source)
- [Prerequisites](https://pytorch.org/get-started/locally/#linux-prerequisites-2)
- [Prerequisites](https://pytorch.org/get-started/locally/#windows-prerequisites)
- [Supported Windows Distributions](https://pytorch.org/get-started/locally/#supported-windows-distributions)
- [Python](https://pytorch.org/get-started/locally/#windows-python)
- [Package Manager](https://pytorch.org/get-started/locally/#windows-package-manager)
- [Installation](https://pytorch.org/get-started/locally/#windows-installation)
- [pip](https://pytorch.org/get-started/locally/#windows-pip)
- [Verification](https://pytorch.org/get-started/locally/#windows-verification)
- [Building from source](https://pytorch.org/get-started/locally/#windows-from-source)
- [Prerequisites](https://pytorch.org/get-started/locally/#windows-prerequisites-2)

## Start Locally [Anchor](https://pytorch.org/get-started/locally/\#start-locally)

Select your preferences and run the install command. Stable represents the most currently tested and supported version of PyTorch. This should
be suitable for many users. Preview is available if you want the latest, not fully tested and supported, builds that are generated nightly.
Please ensure that you have **met the prerequisites below (e.g., numpy)**, depending on your package manager. You can also
[install previous versions of PyTorch](https://pytorch.org/get-started/previous-versions). Note that LibTorch is only available for C++.

**NOTE:** Latest Stable PyTorch requires Python 3.10 or later.

PyTorch Build

Your OS

Package

Language

Compute Platform

Run this Command:

PyTorch Build

Stable (2.14.1)

Preview (Nightly)

Your OS

Linux

Mac

Windows

Package

Pip

LibTorch

Source

Compute Platform

CUDA 12.6

CUDA 13.2

CUDA 12.8

ROCm 7.14

CPU

Run this Command:

```
pip3 install torch torchvision --index-url https://download.pytorch.org/whl/cu126
```

_Could not find the right platform for your hardware?_ See the [PyTorch Additional Platforms](https://pytorch.org/get-started/additional-platforms/) page.

* * *

# Installing on macOS

PyTorch can be installed and used on macOS. Depending on your system and GPU capabilities, your experience with PyTorch on macOS may vary in terms of processing time.

## Prerequisites [Anchor](https://pytorch.org/get-started/locally/\#mac-prerequisites)

### macOS Version [Anchor](https://pytorch.org/get-started/locally/\#macos-version)

PyTorch is supported on macOS 10.15 (Catalina) or above.

### Python [Anchor](https://pytorch.org/get-started/locally/\#mac-python)

It is recommended that you use Python 3.10 - 3.14.
You can install Python either through [Homebrew](https://brew.sh/) or
the [Python website](https://www.python.org/downloads/mac-osx/).

### Package Manager [Anchor](https://pytorch.org/get-started/locally/\#mac-package-manager)

To install the PyTorch binaries, you will need to use the supported package manager: [pip](https://pypi.org/project/pip/).

#### pip [Anchor](https://pytorch.org/get-started/locally/\#pip)

_Python 3_

If you installed Python via Homebrew or the Python website, `pip` was installed with it. If you installed Python 3.x, then you will be using the command `pip3`.

> Tip: If you want to use just the command `pip`, instead of `pip3`, you can symlink `pip` to the `pip3` binary.

## Installation [Anchor](https://pytorch.org/get-started/locally/\#mac-installation)

### pip [Anchor](https://pytorch.org/get-started/locally/\#mac-pip)

To install PyTorch via pip, use the following command, depending on your Python version:

```bash
# Python 3.x
pip3 install torch torchvision
```

Copy

## Verification [Anchor](https://pytorch.org/get-started/locally/\#mac-verification)

To ensure that PyTorch was installed correctly, we can verify the installation by running sample PyTorch code. Here we will construct a randomly initialized tensor.

```python
import torch
x = torch.rand(5, 3)
print(x)
```

Copy

The output should be something similar to:

```plaintext
tensor([[0.3380, 0.3845, 0.3217],\
        [0.8337, 0.9050, 0.2650],\
        [0.2979, 0.7141, 0.9069],\
        [0.1449, 0.1132, 0.1375],\
        [0.4675, 0.3947, 0.1426]])
```

Copy

## Building from source [Anchor](https://pytorch.org/get-started/locally/\#mac-from-source)

For the majority of PyTorch users, installing from a pre-built binary via a package manager will provide the best experience. However, there are times when you may want to install the bleeding edge PyTorch code, whether for testing or actual development on the PyTorch core. To install the latest PyTorch code, you will need to [build PyTorch from source](https://github.com/pytorch/pytorch#from-source).

### Prerequisites [Anchor](https://pytorch.org/get-started/locally/\#mac-prerequisites-2)

1. \[Optional\] Install [pip](https://pypi.org/project/pip/)
2. Follow the steps described here: [https://github.com/pytorch/pytorch#from-source](https://github.com/pytorch/pytorch#from-source)

You can verify the installation as described [above](https://pytorch.org/get-started/locally/#mac-verification).

# Installing on Linux

PyTorch can be installed and used on various Linux distributions. Depending on your system and compute requirements, your experience with PyTorch on Linux may vary in terms of processing time. It is recommended, but not required, that your Linux system has an NVIDIA or AMD GPU in order to harness the full power of PyTorch’s [CUDA](https://developer.nvidia.com/cuda-zone) [support](https://pytorch.org/tutorials/beginner/blitz/tensor_tutorial.html?highlight=cuda&__hstc=76629258.724dacd2270c1ae797f3a62ecd655d50.1746547368336.1746547368336.1746547368336.1&__hssc=76629258.10.1746547368336&__hsfp=2230748894#cuda-tensors) or [ROCm](https://rocm.docs.amd.com/) support.

## Prerequisites [Anchor](https://pytorch.org/get-started/locally/\#linux-prerequisites)

### Supported Linux Distributions [Anchor](https://pytorch.org/get-started/locally/\#supported-linux-distributions)

PyTorch is supported on Linux distributions that use [glibc](https://www.gnu.org/software/libc/) >= v2.28, which include the following:

- [Arch Linux](https://www.archlinux.org/download/), minimum version 2020.01.22
- [CentOS](https://www.centos.org/download/), minimum version 8
- [Debian](https://www.debian.org/distrib/), minimum version 10.0
- [Fedora](https://getfedora.org/), minimum version 24
- [Mint](https://linuxmint.com/download.php), minimum version 20
- [OpenSUSE](https://get.opensuse.org/), minimum version 15
- [PCLinuxOS](https://www.pclinuxos.com/), minimum version 2014.7
- [Slackware](http://www.slackware.com/getslack/), minimum version 14.2
- [Ubuntu](https://www.ubuntu.com/download/desktop), minimum version 20.04 (please note that 20.04 reached EOL)

> The install instructions here will generally apply to all supported Linux distributions. An example difference is that your distribution may support `yum` instead of `apt`. The specific examples shown were run on an Ubuntu 18.04 machine.

### Python [Anchor](https://pytorch.org/get-started/locally/\#linux-python)

Python 3.10-3.14 is generally installed by default on any of our supported Linux distributions, which meets our recommendation.

> Tip: By default, you will have to use the command `python3` to run Python. If you want to use just the command `python`, instead of `python3`, you can symlink `python` to the `python3` binary.

However, if you want to install another version, there are multiple ways:

- APT
- [Python website](https://www.python.org/downloads/mac-osx/)

If you decide to use APT, you can run the following command to install it:

```bash
sudo apt install python
```

Copy

### Package Manager [Anchor](https://pytorch.org/get-started/locally/\#linux-package-manager)

To install the PyTorch binaries, you will need to use the supported package manager: [pip](https://pypi.org/project/pip/).

#### pip [Anchor](https://pytorch.org/get-started/locally/\#pip)

_Python 3_

While Python 3.x is installed by default on Linux, `pip` is not installed by default.

```bash
sudo apt install python3-pip
```

Copy

> Tip: If you want to use just the command `pip`, instead of `pip3`, you can symlink `pip` to the `pip3` binary.

## Installation [Anchor](https://pytorch.org/get-started/locally/\#linux-installation)

### pip [Anchor](https://pytorch.org/get-started/locally/\#linux-pip)

#### No CUDA [Anchor](https://pytorch.org/get-started/locally/\#no-cuda)

To install PyTorch via pip, and do not have a [CUDA-capable](https://developer.nvidia.com/cuda-zone) or [ROCm-capable](https://rocm.docs.amd.com/) system or do not require CUDA/ROCm (i.e. GPU support), in the above selector, choose OS: Linux, Package: Pip, Language: Python and Compute Platform: CPU.
Then, run the command that is presented to you.

#### With CUDA [Anchor](https://pytorch.org/get-started/locally/\#with-cuda)

To install PyTorch via pip, and do have a [CUDA-capable](https://developer.nvidia.com/cuda-zone) system, in the above selector, choose OS: Linux, Package: Pip, Language: Python and the CUDA version suited to your machine. Often, the latest CUDA version is better.
Then, run the command that is presented to you.

#### With ROCm [Anchor](https://pytorch.org/get-started/locally/\#with-rocm)

To install PyTorch via pip, and do have a [ROCm-capable](https://rocm.docs.amd.com/) system, in the above selector, choose OS: Linux, Package: Pip, Language: Python and the ROCm version supported.
Then, run the command that is presented to you.

## Verification [Anchor](https://pytorch.org/get-started/locally/\#linux-verification)

To ensure that PyTorch was installed correctly, we can verify the installation by running sample PyTorch code. Here we will construct a randomly initialized tensor.

```python
import torch
x = torch.rand(5, 3)
print(x)
```

Copy

The output should be something similar to:

```plaintext
tensor([[0.3380, 0.3845, 0.3217],\
        [0.8337, 0.9050, 0.2650],\
        [0.2979, 0.7141, 0.9069],\
        [0.1449, 0.1132, 0.1375],\
        [0.4675, 0.3947, 0.1426]])
```

Copy

Additionally, to check if your GPU driver and CUDA/ROCm is enabled and accessible by PyTorch, run the following commands to return whether or not the GPU driver is enabled (the ROCm build of PyTorch uses the same semantics at the python API level [link](https://github.com/pytorch/pytorch/blob/master/docs/source/notes/hip.rst#hip-interfaces-reuse-the-cuda-interfaces), so the below commands should also work for ROCm):

```python
import torch
torch.cuda.is_available()
```

Copy

## Building from source [Anchor](https://pytorch.org/get-started/locally/\#linux-from-source)

For the majority of PyTorch users, installing from a pre-built binary via a package manager will provide the best experience. However, there are times when you may want to install the bleeding edge PyTorch code, whether for testing or actual development on the PyTorch core. To install the latest PyTorch code, you will need to [build PyTorch from source](https://github.com/pytorch/pytorch#from-source).

### Prerequisites [Anchor](https://pytorch.org/get-started/locally/\#linux-prerequisites-2)

1. Install [Pip](https://pytorch.org/get-started/locally/#pip)
2. If you need to build PyTorch with GPU support
a. for NVIDIA GPUs, install [CUDA](https://developer.nvidia.com/cuda-downloads), if your machine has a [CUDA-enabled GPU](https://developer.nvidia.com/cuda-gpus).
b. for AMD GPUs, install [ROCm](https://rocm.docs.amd.com/), if your machine has a [ROCm-enabled GPU](https://rocm.docs.amd.com/)
3. Follow the steps described here: [https://github.com/pytorch/pytorch#from-source](https://github.com/pytorch/pytorch#from-source)

You can verify the installation as described [above](https://pytorch.org/get-started/locally/#linux-verification).

# Installing on Windows

PyTorch can be installed and used on various Windows distributions. Depending on your system and compute requirements, your experience with PyTorch on Windows may vary in terms of processing time. It is recommended, but not required, that your Windows system has an NVIDIA GPU in order to harness the full power of PyTorch’s [CUDA](https://developer.nvidia.com/cuda-zone) [support](https://pytorch.org/tutorials/beginner/blitz/tensor_tutorial.html?highlight=cuda&__hstc=76629258.724dacd2270c1ae797f3a62ecd655d50.1746547368336.1746547368336.1746547368336.1&__hssc=76629258.10.1746547368336&__hsfp=2230748894#cuda-tensors).

## Prerequisites [Anchor](https://pytorch.org/get-started/locally/\#windows-prerequisites)

### Supported Windows Distributions [Anchor](https://pytorch.org/get-started/locally/\#supported-windows-distributions)

PyTorch is supported on the following Windows distributions:

- [Windows](https://www.microsoft.com/en-us/windows) 7 and greater; [Windows 10](https://www.microsoft.com/en-us/software-download/windows10ISO) or greater recommended.
- [Windows Server 2008](https://docs.microsoft.com/en-us/windows-server/windows-server) r2 and greater

> The install instructions here will generally apply to all supported Windows distributions. The specific examples shown will be run on a Windows 10 Enterprise machine

### Python [Anchor](https://pytorch.org/get-started/locally/\#windows-python)

Currently, PyTorch on Windows only supports Python 3.10-3.14; Python 2.x is not supported.

As it is not installed by default on Windows, there are multiple ways to install Python:

- [Chocolatey](https://chocolatey.org/)
- [Python website](https://www.python.org/downloads/windows/)

> If you decide to use Chocolatey, and haven’t installed Chocolatey yet, ensure that you are running your command prompt as an administrator.

For a Chocolatey-based install, run the following command in an administrative command prompt:

```bash
choco install python
```

Copy

### Package Manager [Anchor](https://pytorch.org/get-started/locally/\#windows-package-manager)

To install the PyTorch binaries, you will need to use the supported package manager: [pip](https://pypi.org/project/pip/).

#### pip [Anchor](https://pytorch.org/get-started/locally/\#pip)

If you installed Python by any of the recommended ways [above](https://pytorch.org/get-started/locally/#windows-python), [pip](https://pypi.org/project/pip/) will have already been installed for you.

## Installation [Anchor](https://pytorch.org/get-started/locally/\#windows-installation)

### pip [Anchor](https://pytorch.org/get-started/locally/\#windows-pip)

#### No CUDA [Anchor](https://pytorch.org/get-started/locally/\#no-cuda)

To install PyTorch via pip, and do not have a [CUDA-capable](https://developer.nvidia.com/cuda-zone) system or do not require CUDA, in the above selector, choose OS: Windows, Package: Pip and CUDA: None.
Then, run the command that is presented to you.

#### With CUDA [Anchor](https://pytorch.org/get-started/locally/\#with-cuda)

To install PyTorch via pip, and do have a [CUDA-capable](https://developer.nvidia.com/cuda-zone) system, in the above selector, choose OS: Windows, Package: Pip and the CUDA version suited to your machine. Often, the latest CUDA version is better.
Then, run the command that is presented to you.

## Verification [Anchor](https://pytorch.org/get-started/locally/\#windows-verification)

To ensure that PyTorch was installed correctly, we can verify the installation by running sample PyTorch code. Here we will construct a randomly initialized tensor.

From the command line, type:

```bash
python
```

Copy

then enter the following code:

```python
import torch
x = torch.rand(5, 3)
print(x)
```

Copy

The output should be something similar to:

```plaintext
tensor([[0.3380, 0.3845, 0.3217],\
        [0.8337, 0.9050, 0.2650],\
        [0.2979, 0.7141, 0.9069],\
        [0.1449, 0.1132, 0.1375],\
        [0.4675, 0.3947, 0.1426]])
```

Copy

Additionally, to check if your GPU driver and CUDA is enabled and accessible by PyTorch, run the following commands to return whether or not the CUDA driver is enabled:

```python
import torch
torch.cuda.is_available()
```

Copy

## Building from source [Anchor](https://pytorch.org/get-started/locally/\#windows-from-source)

For the majority of PyTorch users, installing from a pre-built binary via a package manager will provide the best experience. However, there are times when you may want to install the bleeding edge PyTorch code, whether for testing or actual development on the PyTorch core. To install the latest PyTorch code, you will need to [build PyTorch from source](https://github.com/pytorch/pytorch#from-source).

### Prerequisites [Anchor](https://pytorch.org/get-started/locally/\#windows-prerequisites-2)

1. Install [pip](https://pypi.org/project/pip/)
2. Install [CUDA](https://developer.nvidia.com/cuda-downloads), if your machine has a [CUDA-enabled GPU](https://developer.nvidia.com/cuda-gpus).
3. If you want to build on Windows, Visual Studio with MSVC toolset, and NVTX are also needed. The exact requirements of those dependencies could be found out [here](https://github.com/pytorch/pytorch#from-source).
4. Follow the steps described here: [https://github.com/pytorch/pytorch#from-source](https://github.com/pytorch/pytorch#from-source)

You can verify the installation as described [above](https://pytorch.org/get-started/locally/#windows-verification).

### Docs

Access comprehensive developer documentation for PyTorch

[View Docs ›](https://pytorch.org/docs)

### Tutorials

Get in-depth tutorials for beginners and advanced developers

[View Tutorials ›](https://pytorch.org/tutorials)

### Resources

Find development resources and get your questions answered

[View Resources ›](https://pytorch.org/resources)

## **Stay in touch** for updates, event info, and the latest news

Select Country\*AfghanistanÅland IslandsAlbaniaAlgeriaAmerican SamoaAndorraAngolaAnguillaAntarcticaAntigua and BarbudaArgentinaArmeniaArubaAsia/Pacific RegionAustraliaAustriaAzerbaijanBahamasBahrainBangladeshBarbadosBelarusBelgiumBelizeBeninBermudaBhutanBoliviaBosnia and HerzegovinaBotswanaBouvet IslandBrazilBritish Indian Ocean TerritoryBritish Virgin IslandsBruneiBulgariaBurkina FasoBurundiCambodiaCameroonCanadaCanary IslandsCape VerdeCaribbean NetherlandsCayman IslandsCentral African RepublicChadChileChinaChristmas IslandCocos (Keeling) IslandsColombiaComorosCongoCook IslandsCosta RicaCote d'IvoireCroatiaCubaCuraçaoCyprusCzech RepublicDemocratic Republic of the CongoDenmarkDjiboutiDominicaDominican RepublicEast TimorEcuadorEgyptEl SalvadorEquatorial GuineaEritreaEstoniaEthiopiaEuropeFalkland IslandsFaroe IslandsFijiFinlandFranceFrench GuianaFrench PolynesiaFrench Southern and Antarctic LandsGabonGambiaGeorgiaGermanyGhanaGibraltarGreeceGreenlandGrenadaGuadeloupeGuamGuatemalaGuernseyGuineaGuinea-BissauGuyanaHaitiHeard Island and McDonald IslandsHondurasHong KongHungaryIcelandIndiaIndonesiaIranIraqIrelandIsle of ManIsraelItalyJamaicaJapanJerseyJordanKazakhstanKenyaKiribatiKosovoKuwaitKyrgyzstanLaosLatviaLebanonLesothoLiberiaLibyaLiechtensteinLithuaniaLuxembourgMacauMacedonia (FYROM)MadagascarMalawiMalaysiaMaldivesMaliMaltaMarshall IslandsMartiniqueMauritaniaMauritiusMayotteMexicoMicronesiaMoldovaMonacoMongoliaMontenegroMontserratMoroccoMozambiqueMyanmar (Burma)NamibiaNauruNepalNetherlandsNetherlands AntillesNew CaledoniaNew ZealandNicaraguaNigerNigeriaNiueNorfolk IslandNorth KoreaNorthern Mariana IslandsNorwayOmanPakistanPalauPalestinePanamaPapua New GuineaParaguayPeruPhilippinesPitcairn IslandsPolandPortugalPuerto RicoQatarRéunionRomaniaRussiaRwandaSaint BarthélemySaint HelenaSaint Kitts and NevisSaint LuciaSaint MartinSaint Pierre and MiquelonSaint Vincent and the GrenadinesSamoaSan MarinoSao Tome and PrincipeSaudi ArabiaSenegalSerbiaSeychellesSierra LeoneSingaporeSint MaartenSlovakiaSloveniaSolomon IslandsSomaliaSouth AfricaSouth Georgia and the South Sandwich IslandsSouth KoreaSouth SudanSpainSri LankaSudanSurinameSvalbard and Jan MayenSwazilandSwedenSwitzerlandSyriaTaiwanTajikistanTanzaniaThailandTogoTokelauTongaTrinidad and TobagoTunisiaTürkiyeTurkmenistanTurks and Caicos IslandsTuvaluU.S. Virgin IslandsUgandaUkraineUnited Arab EmiratesUnited KingdomUnited StatesUnited States Minor Outlying IslandsUruguayUzbekistanVanuatuVatican CityVenezuelaVietnamWallis and FutunaWestern SaharaYemenZambiaZimbabwe

By submitting this form, I consent to receive marketing emails from the LF and its projects regarding their events, training, research, developments, and related announcements. I understand that I can unsubscribe at any time using the links in the footers of the emails I receive. [Privacy Policy](https://www.linuxfoundation.org/legal/privacy-policy)

By submitting this form, I consent to receive marketing emails from the LF and its projects regarding their events, training, research, developments, and related announcements. I understand that I can unsubscribe at any time using the links in the footers of the emails I receive. [Privacy Policy](https://www.linuxfoundation.org/privacy/).

© 2026 PyTorch. Copyright © The Linux Foundation®. All rights reserved. The Linux Foundation has registered trademarks and uses trademarks. For more information, including terms of use, privacy policy, and trademark usage, please see our [Policies](https://www.linuxfoundation.org/legal/policies) page. [Trademark Usage](https://www.linuxfoundation.org/trademark-usage). [Privacy Policy](http://www.linuxfoundation.org/privacy)

[Close Menu](https://pytorch.org/get-started/locally/#)

[Menu](https://pytorch.org/get-started/locally/#slide-out-widget-area)

[Back to top](https://pytorch.org/get-started/locally/#) [Cookie Settings](https://pytorch.org/get-started/locally/#)

This website processes personal data

Okay

[Powered by Transcend](https://transcend.io/ "Powered by Transcend")

Do Not Sell My Personal Information

[See our privacy policy](https://www.linuxfoundation.org/legal/privacy-policy "Visit our privacy policy")

Switch language