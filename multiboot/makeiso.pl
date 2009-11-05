#!/usr/bin/perl

system("rm -rf iso");
mkdir("iso");
system("cp kernel.bin iso/");
mkdir("iso/boot");
mkdir("iso/boot/grub");
system("cp iso9660_stage1_5 iso/boot/grub/");
system("cp stage* iso/boot/grub/");
system("cp menu.lst iso/boot/grub/");
chdir("iso");
system("mkisofs -RV \"SIXTY-FOUR\" -b boot/grub/iso9660_stage1_5 -o ../sixty-four.iso -no-emul-boot -boot-load-size 4 -boot-info-table .");
