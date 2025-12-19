import json
import serial
import struct
import time

# --- 配置参数 ---
BLUETOOTH_PORT = 'COM6' #将该串口号改为灯板相应串口
BAUDRATE = 115200
JSON_FILE_PATH = 'led_data.json'

SYNC_BYTE = b'\xAA'
ACK_BYTE = b'\xBB'

def send_animation_data(port, json_path):
    ser = None
    try:
        ser = serial.Serial(port, baudrate=BAUDRATE, timeout=5)
        print(f"成功连接到蓝牙串口: {port}")
        time.sleep(2)

        ser.reset_input_buffer()
        ser.reset_output_buffer()

        with open(json_path, 'r', encoding='utf-8') as f:
            data = json.load(f)

        frame_count = data['frame_count']
        frames = data['frames']
        print(f"解析到 {frame_count} 帧动画数据。")

        frame_data_list = []
        for frame in frames:
            if 'rgb' not in frame or not isinstance(frame['rgb'], list):
                print(f"警告: 帧 {frame.get('index', '未知')} 缺少 'rgb' 数据或格式错误，跳过。")
                continue
            
            frame_binary_data = b''
            frame_binary_data += struct.pack('<f', frame['duration'])
            
            for pixel_list in frame['rgb']:
                if isinstance(pixel_list, list) and len(pixel_list) == 3:
                    r, g, b = pixel_list
                    frame_binary_data += struct.pack('BBB', g, r, b)
                else:
                    print(f"警告: 帧 {frame.get('index', '未知')} 发现无效像素数据: {pixel_list}。")
                    frame_binary_data += struct.pack('BBB', 0, 0, 0)
            frame_data_list.append(frame_binary_data)
        
        print("发送同步字节...")
        ser.write(SYNC_BYTE)
        if ser.read(1) != ACK_BYTE:
            print("错误: 未收到ESP32的同步确认。")
            return

        print("发送总帧数...")
        ser.write(struct.pack('<I', frame_count))
        if ser.read(1) != ACK_BYTE:
            print("错误: 未收到ESP32的总帧数确认。")
            return

        for i, frame_data in enumerate(frame_data_list):
            ser.write(frame_data)
            if ser.read(1) == ACK_BYTE:
                print(f"已发送第 {i+1}/{frame_count} 帧，收到确认。")
            else:
                print(f"错误: 发送第 {i+1} 帧后未收到确认，传输中断。")
                return

        print("\n数据发送完毕！")

    except FileNotFoundError:
        print(f"错误：文件未找到，请检查路径: {json_path}")
    except serial.SerialException as e:
        print(f"错误：无法连接到蓝牙串口或发生通信错误: {e}")
    except Exception as e:
        print(f"发生未知错误: {e}")
    finally:
        if ser and ser.is_open:
            ser.close()
            print("串口已关闭。")

if __name__ == "__main__":
    send_animation_data(BLUETOOTH_PORT, JSON_FILE_PATH)
