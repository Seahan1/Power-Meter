import sys
import re
import serial
from PySide6.QtCore import QTimer, QThread, Signal, QObject
from PySide6.QtWidgets import QApplication, QMainWindow, QVBoxLayout, QWidget
import pyqtgraph as pg

# 1. 串口读取线程（防止 UI 卡顿）
class SerialThread(QThread):
    data_signal = Signal(dict)

    def run(self):
        # 请根据实际情况修改 COM 端口
        try:
            self.ser = serial.Serial('COM3', 115200, timeout=0.1)
            while self.ser.is_open:
                line = self.ser.readline().decode('utf-8').strip()
                if line:
                    # 正则解析：CH:1 V=3.300 I=0.100 P=0.330
                    match = re.search(r"CH:(\d) V=([\d\.-]+) I=([\d\.-]+) P=([\d\.-]+)", line)
                    if match:
                        ch, v, i, p = match.groups()
                        self.data_signal.emit({
                            "ch": int(ch),
                            "v": float(v),
                            "i": float(i) * 1000, # 统一转为 mA
                            "p": float(p) * 1000  # 统一转为 mW
                        })
        except Exception as e:
            print(f"Serial Error: {e}")

# 2. 主窗口
class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("ProPower Dual Channel Monitor (Python)")
        self.resize(1000, 600)

        # 存储数据缓冲区
        self.buf1 = {"v": [], "i": [], "p": []}
        self.buf2 = {"v": [], "i": [], "p": []}

        # 初始化 UI 布局
        layout = QVBoxLayout()
        self.plot1 = pg.PlotWidget(title="Channel 1")
        self.plot2 = pg.PlotWidget(title="Channel 2")
        layout.addWidget(self.plot1)
        layout.addWidget(self.plot2)
        
        container = QWidget()
        container.setLayout(layout)
        self.setCentralWidget(container)

        # 启动串口线程
        self.thread = SerialThread()
        self.thread.data_signal.connect(self.update_data)
        self.thread.start()

    def update_data(self, data):
        target_buf = self.buf1 if data["ch"] == 1 else self.buf2
        target_plot = self.plot1 if data["ch"] == 1 else self.plot2

        # 更新缓冲区（模拟 HTML 中的 MAX_BUFFER）
        target_buf["v"].append(data["v"])
        if len(target_buf["v"]) > 500: target_buf["v"].pop(0)
        
        # 实时刷新曲线
        target_plot.plot(target_buf["v"], clear=True, pen='y')

if __name__ == "__main__":
    app = QApplication(sys.argv)
    # 设置深色主题（简易版）
    app.setPalette(pg.mkQApp().palette()) 
    window = MainWindow()
    window.show()
    sys.exit(app.exec())
