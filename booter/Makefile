include ../Makefile.os_rules

# To protected mode ###############################
pmloader.img: loader.s                          #
	$(AS) -p $(AS_INCLUDE) -o $(BUILD)/$@ $<
### For test  #####################################                                                  
naskfunc.o: loader.s                            #
	$(AS) $(A_FLAG) -p $(AS_INCLUDE) -o $@ $<
###################################################


# Build bootloader ###########################################
ipl10.bin:ipl10.s
	$(AS) -p $(AS_INCLUDE) $< -o $(BUILD)/$@
##############################################################

clean:
	rm -rf *.img
	rm -rf *.bin
