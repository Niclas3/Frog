# Use The Eisenhower Matrix
    IU Important & urgent: Implement all modules described in a specification.
    IN Important & not urgent: Adjust it for user needs, based on the user story.
    NU Not important & urgent: Fill up the specification (use markdown syntax).
    NN Not important & not urgent: Add some extra features.
    *Urgent means that there's 3 days (72 hours) to deadline at least.*

# TODO list
- [x] IU support qemu
    - [x] IU ata-ida support

- [ ] IN thread things
    - [ ] IN user process fork()
  - [ ] IN user process wait()
  - [ ] IN user process exec*()
  - [ ] IN user process kill()

- [ ] IN PIC things
    - [ ] IN pipe

- [ ] IN HID things
  - [ ] NN Put kbd and mouse code to ps2device

- [ ] IN GUI things
  - [x] IU VBE support at qemu
  - [x] IU support 8 bits ascii fonts at VBE mode 
  - [x] IU support mouse (ps/2 mouse)
  - [ ] NN support bdf foramt font at VBE mode
  - [ ] IN GUI Components
    - [ ] IN UIControler
    - [ ] IN Label
    - [ ] IN Buttons
    - [ ] IN Windows
    - [ ] IN Input_components
  - [ ] IN console over windows
  - [ ] NN openGL support

- [ ] NN ATA
    - [ ] NN support DMA
  - [ ] IN impl identify disk function

- [ ] NN Fundamental structures
    - [ ] IU Check ioqueue.c which works at PS2 hid device
  - [ ] IN impl hashmap for configure file

- [ ] NN etc
  - [ ] IN GDT & IDT layout
  - [ ] IN boot up at one disk but not 2 split disks
  - [ ] NN make every things as files

# Feature list
  - [ ] Allows raw physical memory blocks provided by the loader to be used like a block file. Used to provide multiboot payloads as /dev/ram* files.


# Tips

If you want to enlarge core.img size ,should change 3 places
1. Makefile
   1. To change `dd` seek option size.
2. booter/loader.s
   1. when loading kernel.bin arguments.
   2. when loading font.bin start address lba.

