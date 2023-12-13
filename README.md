# TODO list
- [ ] support qemu
    - [*] ata-ida support

- [ ] thread things
    - [ ] user process fork()
  - [ ] user process wait()
  - [ ] user process exec*()
  - [ ] user process kill()

- [ ] PIC things
    - [ ] pipe

- [ ] ATA 
    - [ ] support DMA
  - [ ] impl identify disk function

- [ ] GUI things
  - [*] VBE support at qemu
  - [ ] windows
  - [ ] console over windows

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

