# What is SCSI2Pi?

SCSI2Pi is an alternative to the <a href="https://github.com/PiSCSI/piscsi" target="blank">PiSCSI software</a> for the PiSCSI/RaSCSI board. It provides an improved SCSI emulation, extendend functionality and new tools for the board's initiator mode. SCSI2Pi is compatible with the PiSCSI web UI and the <a href="https://www.scsi2pi.net/en/app.html" target="blank">SCSI Control app</a>.<br />
You can switch from PiSCSI to SCSI2Pi (or back, if needed) in seconds, simply by installing/de-installing a <a href="https://www.scsi2pi.net/en/downloads.html" target="blank">package with the SCSI2Pi binaries</a>. No time-consuming compilation is required.<br />
SCSI2Pi emulates several SCSI or SASI devices like hard drives, CD-ROM drives, printers or network adapters at the same time. You can easily add a range of devices to computers like 68k Macs, Atari ST/TT/Falcon030, Unix workstations, samplers or other computers with SCSI port. Compared to PiSCSI, SCSI2Pi offers <a href="https://www.scsi2pi.net/en/scsi2pi.html" target="blank">numerous extensions</a>, performance improvements and bug fixes. These add to the extensive changes and new features I contributed to the PiSCSI project in the past.<br />
SCSI2Pi was chosen as the name for this project because there are not that many choices left for names that contain both "SCSI" and "Pi" ;-).

# How is SCSI2Pi related to PiSCSI?

In the PiSCSI project there was not much interest in replacing old, often buggy or unnecessary code, or to improve the throughput. In addition, code reviewers were missing, which resulted in a rather long PR backlog, slowing down the development process. Developing software without being able to publish the results is not much fun. Further, long promised features on the PiSCSI roadmap and in tickets have not been addressed. This is why I decided to work on the emulation backend in a separate project. The major part of the PiSCSI C++ codebase has been contributed by me anyway.<br />
There was also no interest in further developing the SCSI emulation and exploiting the initiator mode feature of the FULLSPEC board. This mode, together with new SCSI2Pi command line tools, offers solutions for use cases that have never been addressed before. These tools also help with advanced testing.

# Who am I?

In the past I was the main contributor for the PiSCSI emulation backend. I revised the backend architecture, added a remote interface and re-engineered most of the legacy C++ code. The code was updated to C++-20, which is the latest C++ standard you can currently use on the Pi. All in all this resulted in more modular code and drastically improved the <a href="https://sonarcloud.io/project/overview?id=uweseimet_scsi2pi" target="blank">SonarQube code metrics</a>. Besides adding numerous <a href="https://www.scsi2pi.net/en/scsi2pi.html" target="blank">new features</a> and improving the compatibility with many platforms, I also fixed a range of bugs in the legacy codebase and added an extensive set of unit tests.<br />
I am also the author of the <a href="https://www.scsi2pi.net/en/app.html" target="blank">SCSI Control app</a> for Android, which is the remote control for your PiSCSI/RaSCSI boards. SCSI Control supports both SCSI2Pi and PiSCSI. I also develop the <a href="https://www.hddriver.net" target="blank">HDDRIVER driver package</a> and tools for Atari 32 bit computers.

# SCSI2Pi goals

SCSI2Pi is not meant to completely replace the PiSCSI software, but only the device emulation and the tools. For the PiSCSI project great work is still being done on the web interface, and on supporting users in social media. SCSI2Pi also addresses compatibility issues.<br />
There is no support for the X68000 platform, in particular not for the host bridge (SCBR) device. In PiSCSI the related code has always been in a bad shape, and nobody has been willing to test it. The other PiSCSI features (and more) are supported by SCSI2Pi - many of these I implemented anyway ;-).

# SCSI2Pi website

The <a href="https://www.scsi2pi.net" target="blank">SCSI2Pi website</a> addresses regular users and developers, whereas the information on GitHub is rather developer-centric. The website also provides <a href="https://www.scsi2pi.net/en/downloads.html" target="blank">regular development and release</a> packages. These packages contain the SCSI2Pi binaries, i.e. no time-consuming compilation is required. Installing SCSI2Pi on your Pi is just a matter of seconds.<br />
The website also provides information on the <a href="https://www.scsi2pi.net/en/app.html">SCSI Control app</a> for Android.

# Contributing to SCSI2Pi

If you are interested in SCSI on the Pi and in modern C++ and platform-independent programming you are welcome as a contributor, be it as a developer or a code reviewer.<br />
Have I just said "platform-independent", even though SCSI2Pi is about the Pi? I did indeed, because I ensure that the PiSCSI code also compiles and partially runs on regular Linux PCs and on BSD. This is relevant for developers and for testing, because the faster your development machine, the better. A PC provides a much better development environment than a Pi. My primary development platform is Eclipse CDT on a Linux PC, by the way. Thanks to the <a href="https://github.com/uweseimet/scsi2pi/wiki/Advanced-Testing">SCSI2Pi in-process bus</a> most of the testing does not even require a Pi.
