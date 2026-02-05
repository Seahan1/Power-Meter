import sys
import re
import csv
import serial
import pyqtgraph as pg
from datetime import datetime
from PySide6.QtCore import Qt, QTimer, QThread, Signal, Slot
from PySide6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                             QHBoxLayout, QGridLayout, QLabel, QFrame, 
                             QPushButton, QTextEdit, QFileDialog)

# --- 1. 样式表 (深度还原 HTML 视觉效果) ---
QSS = """
QMainWindow { background-color: #121212; }
QFrame#ScopeWrapper { 
    background: #000000; border: 1px solid #333; border-radius: 8px; 
}
QFrame#Card { 
    background-color: #1e1e1e; border: 1px solid #333; border-radius: 8px; 
}
QLabel { color: #e0e0e0; font-family: 'Segoe UI', 'Consolas'; }
QLabel#Title { font-weight: bold; font-size: 12px; }
QLabel#MeterVal { font-size: 22px; font-weight: bold; font-family: 'Consolas'; }
QLabel#MeterUnit { color: #666; font-size: 11px; }
QTextEdit#LogBox { 
    background-color: #000; border: 1px solid #333; border-radius: 8px;
    color: #aaa; font-family: 'Consolas'; font-size: 11px;
}
QPushButton { 
    background-color: #333; color: white; border: none; padding: 6px; border-radius: 4px;
}
QPushButton:hover { background-color: #444; }
"""

# --- 2. 串口解析线程 ---
class SerialWorker(QThread):
    data_received = Signal(dict)
    log_signal = Signal(str, str)

    def run(self):
        try:
            # 请确认您的串口号 (Windows: 'COMx', Mac: '/dev/cu.usbserial-xxx')
            with serial.Serial('COM3', 115200, timeout=0.1) as ser:
                self.log_signal.emit(">>> 串口已连接", "#00e676")
                while True:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        # 正则匹配 CH:1 V=... I=... P=...
                        m = re.search(r"CH:(\d).*?V=([\d\.-]+).*?I=([\d\.-]+).*?P=([\d\.-]+)", line)
                        if m:
                            ch = int(m.group(1))
                            data = {
                                "ch": ch,
                                "v": float(m.group(2)),
                                "i": float(m.group(3)) * 1000, # A -> mA
                                "p": float(m.group(4)) * 1000  # W -> mW
                            }
                            self.data_received.emit(data)
                            color = "#fdd835" if ch == 1 else "#00e5ff"
                            self.log_signal.emit(line, color)
        except Exception as e:
            self.log_signal.emit(f"错误: {str(e)}", "#f44336")

# --- 3. 主界面 ---
class ProPowerApp(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("ProPower Dual Channel Monitor")
        self.resize(1200, 850)
        self.data_history = {1: [], 2: []} # 用于 CSV 导出
        self.plot_data = {1: {"v": []}, 2: {"v": []}}
        
        self.init_ui()
        self.setStyleSheet(QSS)
        
        self.worker = SerialWorker()
        self.worker.data_received.connect(self.update_all)
        self.worker.log_signal.connect(self.append_log)
        self.worker.start()

    def init_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        main_layout = QHBoxLayout(central)

        # --- 左侧：示波器堆叠 ---
        left_layout = QVBoxLayout()
        self.s1 = self.create_scope("CH 1 LIVE", "#fdd835")
        self.s2 = self.create_scope("CH 2 LIVE", "#00e5ff")
        left_layout.addWidget(self.s1, 1)
        left_layout.addWidget(self.s2, 1)
        
        # 底部控制条
        ctrl_bar = QHBoxLayout()
        self.btn_export = QPushButton("导出 Excel (CSV)")
        self.btn_export.clicked.connect(self.export_csv)
        self.btn_clear = QPushButton("清空数据")
        self.btn_clear.clicked.connect(self.clear_data)
        ctrl_bar.addWidget(self.btn_export)
        ctrl_bar.addWidget(self.btn_clear)
        left_layout.addLayout(ctrl_bar)

        # --- 右侧：侧边面板 ---
        side_panel = QVBoxLayout()
        side_panel.setFixedWidth(300)
        
        self.card1 = self.create_meter_card("CHANNEL 1", "#fdd835", 1)
        self.card2 = self.create_meter_card("CHANNEL 2", "#00e5ff", 2)
        
        self.log_box = QTextEdit()
        self.log_box.setObjectName("LogBox")
        self.log_box.setReadOnly(True)
        
        side_panel.addWidget(self.card1)
        side_panel.addWidget(self.card2)
        side_panel.addWidget(QLabel("SYSTEM LOG"))
        side_panel.addWidget(self.log_box)
        
        main_layout.addLayout(left_layout, 3)
        main_layout.addLayout(side_panel, 1)

    def create_scope(self, title, color):
        frame = QFrame()
        frame.setObjectName("ScopeWrapper")
        layout = QVBoxLayout(frame)
        lbl = QLabel(title)
        lbl.setStyleSheet(f"color: {color}; font-weight: bold; font-size: 10px;")
        layout.addWidget(lbl)
        
        pw = pg.PlotWidget()
        pw.setBackground('#000')
        pw.showGrid(x=True, y=True, alpha=0.3)
        pw.v_curve = pw.plot(pen=pg.mkPen(color, width=2))
        layout.addWidget(pw)
        return pw

    def create_meter_card(self, title, color, ch):
        card = QFrame()
        card.setObjectName("Card")
        card.setStyleSheet(f"QFrame#Card {{ border-left: 4px solid {color}; }}")
        grid = QGridLayout(card)
        
        t_lbl = QLabel(title)
        t_lbl.setObjectName("Title")
        grid.addWidget(t_lbl, 0, 0, 1, 2)

        # 动态创建标签引用
        for i, (name, unit) in enumerate([("V", "V"), ("I", "mA"), ("P", "mW")]):
            grid.addWidget(QLabel(name), i+1, 0)
            val = QLabel("--.---")
            val.setObjectName("MeterVal")
            val.setStyleSheet(f"color: {color};")
            grid.addWidget(val, i+1, 1, Qt.AlignRight)
            grid.addWidget(QLabel(unit), i+1, 2)
            setattr(self, f"ch{ch}_{name.lower()}_lbl", val)
            
        return card

    @Slot(dict)
    def update_all(self, data):
        ch = data['ch']
        # 更新仪表盘
        getattr(self, f"ch{ch}_v_lbl").setText(f"{data['v']:.3f}")
        getattr(self, f"ch{ch}_i_lbl").setText(f"{data['i']:.2f}")
        getattr(self, f"ch{ch}_p_lbl").setText(f"{data['p']:.2f}")
        
        # 更新绘图数据
        buf = self.plot_data[ch]["v"]
        buf.append(data['v'])
        if len(buf) > 500: buf.pop(0)
        
        # 核心：执行绘图更新
        plot_widget = self.s1 if ch == 1 else self.s2
        plot_widget.v_curve.setData(buf)
        
        # 保存历史用于导出
        self.data_history[ch].append(data)

    @Slot(str, str)
    def append_log(self, text, color):
        self.log_box.append(f'<span style="color:{color}">{text}</span>')
        # 自动滚动到底部
        self.log_box.moveCursor(Qt.TextCursor.End)

    def clear_data(self):
        self.plot_data = {1: {"v": []}, 2: {"v": []}}
        self.data_history = {1: [], 2: []}
        self.s1.v_curve.setData([])
        self.s2.v_curve.setData([])
        self.log_box.clear()

    def export_csv(self):
        path, _ = QFileDialog.getSaveFileName(self, "导出数据", "", "CSV Files (*.csv)")
        if path:
            with open(path, 'w', newline='') as f:
                writer = csv.writer(f)
                writer.writerow(["Time", "CH", "Volt(V)", "Curr(mA)", "Pwr(mW)"])
                for ch in [1, 2]:
                    for d in self.data_history[ch]:
                        writer.writerow([datetime.now(), d['ch'], d['v'], d['i'], d['p']])

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = ProPowerApp()
    window.show()
    sys.exit(app.exec())
