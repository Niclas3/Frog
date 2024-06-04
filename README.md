# TODO list
- [x] IU support qemu
    - [x] IU ata-ida support

- [x] IN HID things
  - [x] IU support mouse (ps/2 mouse)
  - [x] NN Put kbd and mouse code to ps2device

- [ ] IN GUI things
  - [x] IU VBE support at qemu
  - [x] IU support 8 bits ascii fonts at VBE mode 
  - [x] NN support bdf foramt font at VBE mode
  - [x] IU GUI Components
    - [ ] IN UIControler
    - [ ] IN Label
    - [ ] IN Buttons
    - [ ] IN Windows
    - [ ] IN Input_components
    - [x] IN mouse GUI (ps/2 mouse)
    - [ ] IU Task bar
  - [ ] IN console over windows
  - [ ] NN openGL support

- [ ] IN thread things
    - [x] IN user process fork()
  - [x] IN user process wait()
  - [x] IN user process exec*()
  - [x] IN user process kill()

- [x] IN PIC things
    - [x] IN pipe

- [ ] NN ATA
    - [ ] NN support DMA
  - [ ] IN impl identify disk function

- [ ] NN Fundamental structures
    - [X] IU ioqueue.c support different type data struct which works at PS2 hid device
  - [ ] IU impl hashmap for configure file

- [ ] NN etc
  - [ ] IN GDT & IDT layout
  - [ ] IN boot up at one disk but not 2 split disks
  - [ ] NN make every things as files

# Feature list
  - [ ] Allows raw physical memory blocks provided by the loader to be used like a block file. Used to provide multiboot payloads as /dev/ram* files.
  - [ ] Support Network
  - [ ] A useful terminal


# Tips
If you want to enlarge core.img size ,should change 3 places
1. Makefile
   1. To change `dd` seek option size.
2. booter/loader.s
   1. when loading kernel.bin arguments.
   2. when loading font.bin start address lba.

