# TODO list
- [x] support qemu
    - [x] ata-ida support

- [ ] thread things
    - [ ] user process fork()
  - [ ] user process wait()
  - [ ] user process exec*()
  - [ ] user process kill()

- [ ] GUI things
  - [x] VBE support at qemu
  - [x] support 8 bits ascii fonts at VBE mode 
  - [ ] support bdf foramt font at VBE mode
  - [ ] support mouse (ps/2 mouse)
  - [ ] GUI Components
    - [ ] UIControler
    - [ ] Label
    - [ ] Buttons
    - [ ] Windows
    - [ ] Input_components
  - [ ] console over windows
  - [ ] openGL support

- [ ] PIC things
    - [ ] pipe

- [ ] ATA
    - [ ] support DMA
  - [ ] impl identify disk function

- [ ] etc
  - [ ] GDT & IDT layout
  - [ ] boot up at one disk but not 2 split disks

# Feature list
 - [ ] Allows raw physical memory blocks provided by the loader to be used like a block file. Used to provide multiboot payloads as /dev/ram* files.


# Tips

If you want to enlarge core.img size ,should change 3 places
1. Makefile
   1. To change `dd` seek option size.
2. booter/loader.s
   1. when loading kernel.bin arguments.
   2. when loading font.bin start address lba.

