import serial
import cv2
import numpy as np
import time
from threading import Thread, Event

class ESP32HiRes:
    def __init__(self, port='/dev/cu.usbserial-1130', baudrate=115200):
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        self.streaming = False
        self.current_frame = None
        self.frame_lock = Event()
        self.thread = None
        
    def conectar(self):
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=2)
            self.ser.setDTR(False)
            self.ser.setRTS(False)
            time.sleep(3.0)
            
            if self.ser.in_waiting:
                self.ser.read(self.ser.in_waiting)
                
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()
            return True
        except Exception as e:
            return False
    
    def iniciar_stream(self):
        if not self.ser:
            return False
        
        try:
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()
            self.ser.write(b"STREAM START\n")
            self.ser.flush()
            time.sleep(0.5)
            
            self.ser.read(self.ser.in_waiting if self.ser.in_waiting > 0 else 50)
            
            self.streaming = True
            self.thread = Thread(target=self._read_stream_vga, daemon=True)
            self.thread.start()
            return True
        except Exception as e:
            return False
    
    def detener_stream(self):
        if self.ser:
            try:
                self.ser.write(b"STREAM STOP\n")
                self.streaming = False
                time.sleep(0.3)
                if self.thread and self.thread.is_alive():
                    self.thread.join(timeout=1.0)
            except Exception as e:
                pass
    
    def capturar_con_filtro(self, filtro='original'):
        if not self.ser:
            return None
        
        try:
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()
            
            cmd = f"CAPTURE {filtro}\n"
            self.ser.write(cmd.encode())
            self.ser.flush()
            
            buffer = b''
            start_marker = b'CAPTURE_START'
            end_marker = b'CAPTURE_END'
            
            self.ser.read_all()
            
            timeout_start = time.time()
            timeout = 8
            
            while time.time() - timeout_start < timeout:
                chunk = self.ser.read(4096)
                if chunk:
                    buffer += chunk
                    
                    if start_marker in buffer and end_marker in buffer:
                        start_idx = buffer.index(start_marker) + len(start_marker)
                        end_idx = buffer.index(end_marker)
                        
                        jpeg_data = buffer[start_idx:end_idx].strip()
                        img_array = np.frombuffer(jpeg_data, dtype=np.uint8)
                        frame = cv2.imdecode(img_array, cv2.IMREAD_COLOR)
                        
                        if frame is not None:
                            return frame
                        else:
                            buffer = buffer[end_idx + len(end_marker):]
            
            return None
            
        except Exception as e:
            return None
    
    def _read_stream_vga(self):
        buffer = b''
        
        while self.streaming:
            try:
                chunk = self.ser.read(4096)
                if not chunk:
                    time.sleep(0.01)
                    continue
                
                buffer += chunk
                
                while len(buffer) > 4:
                    start_idx = buffer.find(b'\xFF\xD8')
                    if start_idx == -1:
                        buffer = buffer[-2:] if len(buffer) > 2 else buffer
                        break
                    
                    end_idx = buffer.find(b'\xFF\xD9', start_idx + 2)
                    if end_idx == -1:
                        buffer = buffer[start_idx:]
                        break
                    
                    jpeg_data = buffer[start_idx:end_idx + 2]
                    buffer = buffer[end_idx + 2:]
                    
                    try:
                        frame = cv2.imdecode(np.frombuffer(jpeg_data, dtype=np.uint8), cv2.IMREAD_COLOR)
                        if frame is not None:
                            self.current_frame = frame
                            self.frame_lock.set()
                    except:
                        pass
                        
            except Exception as e:
                if self.streaming:
                    time.sleep(0.1)
    
    def obtener_frame(self, timeout=0.5):
        if self.frame_lock.wait(timeout=timeout):
            self.frame_lock.clear()
            return self.current_frame
        return None
    
    def desconectar(self):
        self.detener_stream()
        if self.ser:
            try:
                self.ser.close()
            except:
                pass
