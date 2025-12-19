import cv2
import numpy as np
import json

def process_video(input_path, output_video, output_data):
    # 打开视频
    cap = cv2.VideoCapture(input_path)
    if not cap.isOpened():
        print("无法打开视频文件")
        return

    # 获取原视频信息
    orig_width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    orig_height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    orig_fps = cap.get(cv2.CAP_PROP_FPS)
    print(f"原始分辨率: {orig_width}x{orig_height}, 帧率: {orig_fps:.2f}fps")

    # 输入目标参数
    target_width = 8
    target_height = 8
    target_fps = float(input("请输入目标帧率: "))

    total_leds = 64 
    target_leds = target_width * target_height

    # 视频写出对象
    fourcc = cv2.VideoWriter_fourcc(*'mp4v')
    out = cv2.VideoWriter(output_video, fourcc, target_fps, (target_width, target_height))

    frame_data = []
    frame_interval = int(round(orig_fps / target_fps)) if target_fps < orig_fps else 1
    frame_idx = 0

    while True:
        ret, frame = cap.read()
        if not ret:
            break

        # 采样帧率：跳帧
        if frame_idx % frame_interval != 0:
            frame_idx += 1
            continue

        # 缩放视频到目标分辨率
        resized = cv2.resize(frame, (target_width, target_height), interpolation=cv2.INTER_AREA)
        out.write(resized)

        # 转RGB，按LED数量生成数组
        rgb_frame = cv2.cvtColor(resized, cv2.COLOR_BGR2RGB)
        led_colors = rgb_frame.reshape(-1, 3).tolist()

        # 填黑多余LED
        if len(led_colors) < total_leds:
            led_colors = led_colors + [[0, 0, 0]] * (total_leds - len(led_colors))
        elif len(led_colors) > total_leds:
            led_colors = led_colors[:total_leds]

        
        frame_info = {
            "index": frame_idx,
            "duration": 1.0 / target_fps,
            "rgb": led_colors
        }

        frame_data.append(frame_info)
        frame_idx += 1

    with open(output_data, "w") as f:
        json.dump({
            "frame_count": len(frame_data),
            "frames": frame_data
        }, f, indent=2)

    cap.release()
    out.release()
    print(f"转换完成！输出视频: {output_video}, 输出LED数据: {output_data}")

if __name__ == "__main__":
    filename = input("请输入待转换视频名称（如 example.mp4）：")
    process_video(filename, "compressed.mp4", "led_data.json")
