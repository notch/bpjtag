// =======================================
// WRT54G EJTAG DeBrick Utility   v4.0
// =======================================

*** New for v4.0 *** 

* Software re-written to support 38 flash chips and auto-detect flash chip 
  and flash size and adjust region info accordingly for reading/writing to the
  flash chips.  Also added support for compiling under Windows, Linux, and FreeBSD.
  It also now detects the new Rev 2 of the bcm4712 chip as well as the new
  Broadcom BCM5352 chip.
  
* Two new switches were added: /nobreak and /fc:XX
  /nobreak will prevent issuing an EJTAG Debug Mode JTAGBRK
  /fc:XX will specify XX flash chip manually (instead of Auto-Detection of the Flash Chip)
  
  The following flash chips (switch options) are supported:
  
           /fc:01 ..... AMD 29lv160DB 1Mx16 BotB   (2MB)
           /fc:02 ..... AMD 29lv160DT 1Mx16 TopB   (2MB)
           /fc:03 ..... AMD 29lv320DB 2Mx16 BotB   (4MB)
           /fc:04 ..... AMD 29lv320DT 2Mx16 TopB   (4MB)
           /fc:05 ..... AMD 29lv320MB 2Mx16 BotB   (4MB)
           /fc:06 ..... AMD 29lv320MT 2Mx16 TopB   (4MB)
           /fc:07 ..... AMD 29lv320MT 2Mx16 TopB   (4MB)
           /fc:08 ..... Intel 28F128J3 8Mx16       (16MB)
           /fc:09 ..... Intel 28F160B3 1Mx16 BotB  (2MB)
           /fc:10 ..... Intel 28F160B3 1Mx16 TopB  (2MB)
           /fc:11 ..... Intel 28F160C3 1Mx16 BotB  (2MB)
           /fc:12 ..... Intel 28F160C3 1Mx16 TopB  (2MB)
           /fc:13 ..... Intel 28F160S3/5 1Mx16     (2MB)
           /fc:14 ..... Intel 28F320B3 2Mx16 BotB  (4MB)
           /fc:15 ..... Intel 28F320B3 2Mx16 TopB  (4MB)
           /fc:16 ..... Intel 28F320C3 2Mx16 BotB  (4MB)
           /fc:17 ..... Intel 28F320C3 2Mx16 TopB  (4MB)
           /fc:18 ..... Intel 28F320J3 2Mx16       (4MB)
           /fc:19 ..... Intel 28F320J5 2Mx16       (4MB)
           /fc:20 ..... Intel 28F320S3/5 2Mx16     (4MB)
           /fc:21 ..... Intel 28F640B3 4Mx16 BotB  (8MB)
           /fc:22 ..... Intel 28F640B3 4Mx16 TopB  (8MB)
           /fc:23 ..... Intel 28F640C3 4Mx16 BotB  (8MB)
           /fc:24 ..... Intel 28F640C3 4Mx16 TopB  (8MB)
           /fc:25 ..... Intel 28F640J3 4Mx16       (8MB)
           /fc:26 ..... Intel 28F640J5 4Mx16       (8MB)
           /fc:27 ..... MBM29LV320BE 2Mx16 BotB    (4MB)
           /fc:28 ..... MBM29LV320TE 2Mx16 TopB    (4MB)
           /fc:29 ..... MX29LV320B 2Mx16 BotB      (4MB)
           /fc:30 ..... MX29LV320B 2Mx16 BotB      (4MB)
           /fc:31 ..... MX29LV320T 2Mx16 TopB      (4MB)
           /fc:32 ..... MX29LV320T 2Mx16 TopB      (4MB)
           /fc:33 ..... SST39VF320 2Mx16           (4MB)
           /fc:34 ..... ST 29w320DB 2Mx16 BotB     (4MB)
           /fc:35 ..... ST 29w320DT 2Mx16 TopB     (4MB)
           /fc:36 ..... Sharp 28F320BJE 2Mx16 BotB (4MB)
           /fc:37 ..... TC58FVB321 2Mx16 BotB      (4MB)
           /fc:38 ..... TC58FVT321 2Mx16 TopB      (4MB)  
           
* If you have trouble with auto-detection or just running the software try using both
  the /noreset and /nobreak options together.
  
* In this version I have gone back to the normal flash window of 0x1FC00000 for the
  smaller flash chips (2MB and 4MB), while the larger flash chips (8MB and 16MB) will 
  use the flash window at 0x1C000000.
  
* Included are the source files for Compiling under Linux (default), Windows, FreeBSD.
  Also included are a pre-compiled version (wrt54g.exe) for Windows, along with the
  GiveIO.sys driver and installer which are need ONLY for use with Windows.  

* I have also included the original "HairyDairyMaid_WRT54G_v2_DeBrick_Guide.pdf"
  for reference in creating the needed JTAG cable and to give some basic
  understanding of debricking your router / flashing via EJTAG.  Note that
  in this version some of the command line switches have changed from what is
  represented in the "HairyDairyMaid_WRT54G_v2_DeBrick_Guide.pdf" doc.  I am
  not going to go back and re-write that doc! (I hate writing documentation) :)

* Many thanks to Terry Porter for creating an easy to read schematic
  (jtag-hairydairymaid.png) that I have also included here for reference 
  in building the JTAG cable.  (As info - it shows GND being connected to 
  pin 6 on the JTAG header side and the .doc show it going to pin 2 as 
  either work just the same).

* Thanks to Daniel O'Connor for the FreeBSD support suggestion/code.

Enjoy!               

-hairydairydairy (a.k.a lightbulb)



----------------------------------------------------------------------------------------------

*** Installing GiveIO.sys is only needed if you will be the DeBrick Utility under Windows. ***

----------------------------------------------------------------------------------------------

========================================
Installing GiveIO on Windows XP
========================================

1. Run the LoadDrv utility with giveio.sys in the same directory and click the
   install button.  This should copy giveio.sys to the systems directory, but 
   I've found that it doesn't. Alternatively, just copy giveio.sys to 
   C:\WINDOWS\system32\drivers.

2. In the LoadDrv utility, enter the full pathname of the location of giveio.sys
   (for example, c:\windows\system32\drivers\giveio.sys).

3. In LoadDrv, click the Start button.

4. This driver should now be started. If you want the driver to start whenever 
   the computer is restarted proceed with the following steps.

5. In the Control Panel, open System and go to the hardware tab.

6. Click on the Device Manager button. This will open a new window.

7. In the Device Manager window, click on the View menu and select 
   "Show hidden devices."  This will reveal a Non-Plug and Play Drivers icon 
   in the file tree.

8. Expand the Non-Plug and Play Drivers tree.

9. Find and right click giveio and select Properties from the popup menu. 
   This will bring up a window of the giveio Properties.

10. In the Properties window, select the Driver tab.

11. Select Automatic from the dropdown box for the type.

12. This change will take effect after you reboot the computer.

----------------------------------------------------------------------------------------------
