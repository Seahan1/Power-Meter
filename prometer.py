import sys
import re
import serial
import pyqtgraph as pg
from PySide6.QtCore import Qt, QTimer, QThread, Signal
from PySide6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                             QHBoxLayout, QGridLayout, QLabel, QFrame, QPushButton)

# --- 样式定义 (等同于你的 CSS) ---
DARK_STYLE = """
QMainWindow { background-color: #121212; }
QFrame#Card { 
    background-color: #1e1e1e; 
    border: 1px solid #333; 
    border-radius: 8px; 
    margin: 5px;
}
QLabel#MeterVal { 
    font-size: 24px; 
    font-weight: bold; 
    font-family: 'Consolas';
}
QLabel#MeterUnit { color: #666; font-size: 12px; }
"""

class ProPowerApp(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("ProPower Dual Channel Monitor")
        self.resize(1200, 800)
        
        # 数据存储
        self.data_buf = {1: {"v": [], "i": [], "p": []}, 2: {"v": [], "i": [], "p": []}}
        self.max_points = 500

        self.init_ui()
        self.setStyleSheet(DARK_STYLE)

    def init_ui(self):
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QHBoxLayout(central_widget)

        # --- 左侧：示波器区域 ---
        left_layout = QVBoxLayout()
        self.plot1 = self.create_plot("CH 1", "#fdd835") # 黄色 V
        self.plot2 = self.create_plot("CH 2", "#00e5ff") # 青色 V
        left_layout.addWidget(self.plot1)
        left_layout.addWidget(self.plot2)
        
        # --- 右侧：控制面板 ---
        right_panel = QVBoxLayout()
        self.card1 = self.create_meter_card("CHANNEL 1", "#fdd835")
        self.card2 = self.create_meter_card("CHANNEL 2", "#00e5ff")
        right_panel.addWidget(self.card1)
        right_panel.addWidget(self.card2)
        right_panel.addStretch() # 填充底部

        main_layout.addLayout(left_layout, stretch=3)
        main_layout.addLayout(right_panel, stretch=1)

    def create_plot(self, title, color):
        p = pg.PlotWidget(title=title)
        p.setBackground('#000000')
        p.showGrid(x=True, y=True, alpha=0.3)
        # 预创建曲线对象，避免在 update 时重复创建
        p.v_curve = p.plot(pen=pg.mkPen(color, width=2))
        return p

    def create_meter_card(self, title, accent_color):
        card = QFrame()
        card.setObjectName("Card")
        card.setFixedWidth(280)
        # 设置左侧彩色边框线
        card.setStyleSheet(f"QFrame#Card {{ border-left: 4px solid {accent_color}; }}")
        
        layout = QGridLayout(card)
        title_label = QLabel(title)
        title_label.setStyleSheet(f"color: {accent_color}; font-weight: bold;")
        layout.addWidget(title_label, 0, 0, 1, 2)

        # 电压显示
        layout.addWidget(QLabel("VOLT"), 1, 0)
        v_val = QLabel("--.---")
        v_val.setObjectName("MeterVal")
        v_val.setStyleSheet(f"color: {accent_color};")
        layout.addWidget(v_val, 1, 1, Qt.AlignRight)
        
        # 保存引用以便更新
        setattr(self, f"v_label_{title[-1]}", v_val)
        return card

    def update_ui(self, data):
        ch = data['ch']
        buf = self.data_buf[ch]
        
        # 更新数据缓冲
        buf['v'].append(data['v'])
        if len(buf['v']) > self.max_points: buf['v'].pop(0)

        # 1. 更新数值 (等同于你的 updateMeters)
        if hasattr(self, f"v_label_{ch}"):
            getattr(self, f"v_label_{ch}").setText(f"{data['v']:.3f}")

        # 2. 更新绘图 (直接更新 Data，性能最高)
        plot_widget = self.plot1 if ch == 1 else self.plot2
        plot_widget.v_curve.setData(buf['v'])

# --- 启动逻辑 ---
if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = ProPowerApp()
    window.show()
    
    # 模拟数据测试 (如果没有硬件连接，可以用这个看效果)
    # timer = QTimer()
    # timer.timeout.connect(lambda: window.update_ui({"ch":1, "v": 3.3 + (0.1 * __import__('random').random())}))
    # timer.start(50)
    
    sys.exit(app.exec())
