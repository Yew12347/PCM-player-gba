# PCM-player-gba
play pcm file on a gba Built on top of a patched version of butano engine

# Note

this project is still wip but now work on real hardware too just a little bit of glitch but it bareable

# How to build
1. install devkitpro or devkitarm up to os
2. install pillow python package
```sh
# For MSYS2/MinGW-w64 users
pacman -S mingw-w64-x86_64-python-pillow
# For WSL2/Ubuntu/Debian users
sudo apt-get install python3-pil
# For Mac users
brew install pillow
# For FreeBSD users
pkg install py38-pillow
# For CentOS users
yum install python3-pillow
# For Fedora Linux users
dnf install python3-pillow
# For Arch Linux users
pacman -S python37-pillow
```
or on windows
```cmd
pip install pillow
```
3. cd to pcm player source(im not telling this basic of a thing bro)
4. run make
