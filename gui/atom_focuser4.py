import os
import tkinter as tk
import customtkinter as ctk
import serial
from serial.tools import list_ports
import time
import threading
from logging import NullHandler

FONT_TYPE = "meiryo"

debug = False

class App(ctk.CTk):

    def __init__(self):
        super().__init__()
        self.W_STEP_MIN_C = 10
        self.W_STEP_MID_C = 50
        self.W_STEP_MAX_C = 100
        self.AUTO_POFF_MIN = 5

        self.tm = NullHandler

        self.module_en = False

        self.ser = None
        self.cur_pos = 0

        # メンバー変数の設定
        self.fonts = (FONT_TYPE, 15)
        # フォームサイズ設定
        self.geometry("375x150")
        self.title("ATOM fofuser4")

        # フォームのセットアップをする
        self.setup_form()

        # イベント処理
        self.bind("<Control-MouseWheel>", self.wheelScrollingMin)
        self.bind("<MouseWheel>", self.wheelScrollingMid)
        self.bind("<Shift-MouseWheel>", self.wheelScrollingMax)
        self.bind("<Control-Up>", self.cursorUpMin)
        self.bind("<Up>", self.cursorUpMid)
        self.bind("<Shift-Up>", self.cursorUpMax)
        self.bind("<Control-Down>", self.cursorDownMin)
        self.bind("<Down>", self.cursorDownMid)
        self.bind("<Shift-Down>", self.cursorDownMax)
        self.bind("<space>", self.markPoint1)
        self.bind("<Shift-space>", self.markPoint2)
        self.bind("<c>", self.setCenterOfMarkE)
    
    # Cursor / Mouse bind
    def cursorUpMin(self, event):
        self.moveStep(-self.W_STEP_MIN_C)

    def cursorUpMid(self, event):
        self.moveStep(-self.W_STEP_MID_C)

    def cursorUpMax(self, event):
        self.moveStep(-self.W_STEP_MAX_C)

    def cursorDownMin(self, event):
        self.moveStep( self.W_STEP_MIN_C)

    def cursorDownMid(self, event):
        self.moveStep( self.W_STEP_MID_C)

    def cursorDownMax(self, event):
        self.moveStep( self.W_STEP_MAX_C)

    def wheelScrollingMin(self, event):
        if (event.delta > 0):
            self.moveStep( self.W_STEP_MIN_C)
        else:
            self.moveStep(-self.W_STEP_MIN_C)

    def wheelScrollingMid(self, event):
        if (event.delta > 0):
            self.moveStep( self.W_STEP_MID_C)
        else:
            self.moveStep(-self.W_STEP_MID_C)

    def wheelScrollingMax(self, event):
        if (event.delta > 0):
            self.moveStep( self.W_STEP_MAX_C)
        else:
            self.moveStep(-self.W_STEP_MAX_C)
    
    def updatePorts(self):
        ser_ports = list_ports.comports()   # Get port list
        devices = [info.device for info in ser_ports]
        return devices
    
    def markPoint1(self, event):
        self.label_mk2_1.configure(text = self.label_mk1_1.cget("text"))
        self.label_mk1_1.configure(text = str(self.cur_pos))

    def markPoint2(self, event):
        self.label_mk2_1.configure(text = str(self.cur_pos))

    def setCenterOfMark(self):
        new_pos = int((int(self.label_mk1_1.cget("text")) + int(self.label_mk2_1.cget("text"))) / 2)
        print("C:" + str(new_pos))
        self.moveStep(new_pos - self.cur_pos)

    def setCenterOfMarkE(self, event):
        self.setCenterOfMark()

    def tm_stop(self):
        self.tm.cancel()
        del self.tm
        self.tm = NullHandler

    def tm_start(self):
        self.tm = threading.Timer(self.AUTO_POFF_MIN * 60 , self.tm_callback)
        self.tm.start()

    def tm_callback(self):
        self.tm_stop()
        self.sendCmd("D\n")
        self.module_en = False
        print("Module disabled")
            
    # Serial port
    def openClose(self):
        com_sel = self.ser_port_menu.get()
        if com_sel != "":
            if self.ser == None:
                self.ser = serial.Serial(com_sel, 115200, timeout = None)
                if self.ser != None:
                    print("Open: " + com_sel)
                    self.sw_connect.select()
                    with open("atom_focuser4.conf", mode='w') as f:
                        f.write(com_sel)
                    self.sendCmd("E\n")
                    self.module_en = True
                    print("Module enabled")
                    self.tm_start()
                else:
                    self.sw_connect.deselect()
                    print("Cannot open: " + com_sel)
                    tk.messagebox.showerror('Error', "Can't open " + com_sel)
            else:
                self.tm_stop()
                self.ser.close()
                self.ser = None
                print("Close: " + com_sel)
                self.sw_connect.deselect()

    def sendCmd(self, str):
        global debug
        if debug == True:
            return True
        if self.ser != None:
            self.ser.write(str.encode())
            rcv = self.ser.readline()
            # print(rcv.decode(), end="")
            return True
        else:
            tk.messagebox.showerror('Error', "Please connect a serial port")
            return False
        
    def moveStep(self, step):
        if not self.module_en:
            self.sendCmd("E\n")
            self.module_en = True
            print("Module enabled")
        else:
            self.tm_stop()
        snd = str(step) + '\n'
        if self.sendCmd(snd):
            self.cur_pos += int(step)
            self.entry_pos.delete(0, tk.END)
            self.entry_pos.insert(tk.END, str(self.cur_pos))
        self.tm_start()

    def inputPos(self, entry):
        new_pos = int(self.entry_pos.get())
        self.moveStep(new_pos - self.cur_pos)

    def setup_form(self):
        # Fundamental    
        ctk.set_appearance_mode("dark")  # Modes: system (default), light, dark
        ctk.set_default_color_theme("blue")  # Themes: blue (default), dark-blue, green

        self.grid_rowconfigure(0, weight=1)

        # Top frame
        self.frame_1 = ctk.CTkFrame(self)
        self.frame_1.grid(row=0, column=0, sticky="ew")
        self.frame_3 = ctk.CTkFrame(self)
        self.frame_3.grid(row=1, column=0)
        self.frame_2 = ctk.CTkFrame(self)
        self.frame_2.grid(row=2, column=0)

        # Frame1
        # Seriap port
        devs = self.updatePorts()
        self.ser_port_menu = ctk.CTkOptionMenu(self.frame_1, values=devs)
        self.ser_port_menu.grid(row=0, column=0, padx=5, pady=5, sticky="ew")
        if os.path.isfile("atom_focuser4.conf"):
            with open("atom_focuser4.conf", mode='r') as f:
                last_dev = f.read()
                for i in devs:
                    if i == last_dev:
                        self.ser_port_menu.set(last_dev)
        self.switch_var = ctk.StringVar(value="on")
        self.sw_connect = ctk.CTkSwitch(self.frame_1, text="Connect", command=self.openClose, onvalue="on", offvalue="off")
        self.sw_connect.grid(row=0, column=1, padx=5, pady=5, sticky="ew")

        # Current position
        self.label_pos = ctk.CTkLabel(self.frame_1, text="Position:")
        self.label_pos.grid(row=1, column=0, padx=5, pady=5, sticky="ew")
        self.entry_pos = ctk.CTkEntry(self.frame_1, placeholder_text=str(self.cur_pos))
        self.entry_pos.grid(row=1, column=1, padx=5, pady=5, sticky="ew")
        self.entry_pos.bind('<Return>', command=self.inputPos)
        # self.btn_scroll = ctk.CTkButton(self.frame_1, text="10", width=50, command=self.setScrollStep)
        # self.btn_scroll.grid(row=1, column=2, padx=5, pady=5, sticky="ew")

        # Frame3
        self.label_mk1_0 = ctk.CTkLabel(self.frame_3, text="M1:")
        self.label_mk1_0.grid(row=0, column=0, padx=5, pady=5, sticky="ew")
        self.label_mk1_1 = ctk.CTkLabel(self.frame_3, text="0", width=100)
        self.label_mk1_1.grid(row=0, column=1, padx=5, pady=5, sticky="ew")
        self.label_mk2_0 = ctk.CTkLabel(self.frame_3, text="M2:")
        self.label_mk2_0.grid(row=0, column=2, padx=5, pady=5, sticky="ew")
        self.label_mk2_1 = ctk.CTkLabel(self.frame_3, text="0", width=100)
        self.label_mk2_1.grid(row=0, column=3, padx=5, pady=5, sticky="ew")
        self.btn_mk12 = ctk.CTkButton(self.frame_3, text="(M1+M2)/2", width=50, command=self.setCenterOfMark)
        self.btn_mk12.grid(row=0, column=4, padx=5, pady=5, sticky="ew")

        # Frame2
        # Control buttons
        # Rev
        self.btn_ml = ctk.CTkButton(self.frame_2, text="-100", width=50, command=lambda:self.moveStep(-100))
        self.btn_ml.grid(row=0, column=0, padx=5, pady=5, sticky="ew")
        self.btn_mm = ctk.CTkButton(self.frame_2, text="-50",  width=50, command=lambda:self.moveStep(-50))
        self.btn_mm.grid(row=0, column=1, padx=5, pady=5, sticky="ew")
        self.btn_ms = ctk.CTkButton(self.frame_2, text="-10",  width=50, command=lambda:self.moveStep(-10))
        self.btn_ms.grid(row=0, column=2, padx=5, pady=5, sticky="ew")
        # Fwd
        self.btn_pl = ctk.CTkButton(self.frame_2, text="+100", width=50, command=lambda:self.moveStep(100))
        self.btn_pl.grid(row=0, column=5, padx=5, pady=5, sticky="ew")
        self.btn_pm = ctk.CTkButton(self.frame_2, text="+50",  width=50, command=lambda:self.moveStep(50))
        self.btn_pm.grid(row=0, column=4, padx=5, pady=5, sticky="ew")
        self.btn_ps = ctk.CTkButton(self.frame_2, text="+10",  width=50, command=lambda:self.moveStep(10))
        self.btn_ps.grid(row=0, column=3, padx=5, pady=5, sticky="ew")

if __name__ == "__main__":
    # アプリケーション実行
    app = App()
    app.mainloop()
