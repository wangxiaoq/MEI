# RAMpage - To Validate MEI(Memory Error Injection)a

*Wang Xiaoqiang<wang_xiaoq@126.com>*

This repository is originally from https://github.com/schirmeier/rampage.
We change the user space part to a more simple program `userspace.c' to 
validate the [MEI](https://github.com/wangxiaoq/MEI). 

And We will continue to update RAMpage in [this repository](https://github.com/wangxiaoq/rampage).

## Basic Setup

These steps are required to setup and run RAMpage:

* Download the linux kernel 4.4 source code:

  ```
  wget https://www.kernel.org/pub/linux/kernel/v4.x/linux-4.4.tar.xz
  ```

* Uncompress the kernel code:

  ```
  tar xvf linux-4.4.tar.xz
  ```
  
* Patch the kernel with rampage patch:

  ```
  cd linux-4.4 
  patch -p1 < rampage/patches/kernelpatch-rampage-4.4.diff
  ```

* Copy a config file from current running system to kernel source code:

  ```
  cp /boot/config-<kernel-version> .config
  ```

  Note: *kernel-version* may not equal 4.4, but it's ok, just do it:).

* Compile the kernel:

  ```
  make menuconfig 
  make 
  make modules_install 
  make install 
  ```
  
* After having booted the patched kernel, you need to build and insert
  the kernel module by entering module/ and running ```./rebuild.sh```.

* Use MEI to inject errors into the system(https://github.com/wangxiaoq/MEI).

* Compile the userspace.c in RAMpage:

  ```
  gcc userspace.c -o userspace
  ```

* Use RAMpage to detect injected errors:

  ```
  sudo ./userspace
  ```


##LICENSE
The code in this repository is licensed under the GNU General Public License
(GPL), Version 2.
