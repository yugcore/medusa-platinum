import os
import xml.etree.ElementTree as ET

ICONS = {
    # Tool Palette (Left Strip)
    "tool_select": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M4 2.5L18.5 12L11.5 14L15.5 21.5L12 23.5L8 16L4 20V2.5Z"/>
</svg>''',

    "tool_move": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M12 1L16 5H13V11H19V8L23 12L19 16V13H13V19H16L12 23L8 19H11V13H5V16L1 12L5 8V11H11V5H8L12 1Z"/>
</svg>''',

    "tool_rotate": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M12 2C6.48 2 2 6.48 2 12C2 14.5 2.9 16.8 4.4 18.5L6.6 16.3C5.6 15.1 5 13.6 5 12C5 8.13 8.13 5 12 5V2L16 6L12 10V7C9.24 7 7 9.24 7 12C7 13.2 7.43 14.3 8.15 15.15L10.35 12.95C10.13 12.66 10 12.34 10 12C10 10.9 10.9 10 12 10C13.1 10 14 10.9 14 12C14 12.34 13.87 12.66 13.65 12.95L15.85 15.15C16.57 14.3 17 13.2 17 12C17 9.24 14.76 7 12 7V4L16 8L12 12V9"/>
  <path d="M12 22C17.52 22 22 17.52 22 12C22 9.5 21.1 7.2 19.6 5.5L17.4 7.7C18.4 8.9 19 10.4 19 12C19 15.87 15.87 19 12 19V22L8 18L12 14V17C14.76 17 17 14.76 17 12C17 10.8 16.57 9.7 15.85 8.85L13.65 11.05C13.87 11.34 14 11.66 14 12C14 13.1 13.1 14 12 14C10.9 14 10 13.1 10 12C10 11.66 10.13 11.34 10.35 11.05L8.15 8.85C7.43 9.7 7 10.8 7 12C7 14.76 9.24 17 12 17V20L8 16L12 12V15"/>
</svg>''',

    "tool_scale": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M2 2H9V5H5.12L9.56 9.44L7.44 11.56L3 7.12V11H0V2H2ZM15 2H22V9H19V5.12L14.56 9.56L12.44 7.44L16.88 3H13V0H22V2ZM2 15H5V18.88L9.44 14.44L11.56 16.56L7.12 21H11V24H2V15ZM22 22H15V19H18.88L14.44 14.56L16.56 12.44L21 16.88V13H24V22H22Z"/>
</svg>''',

    "tool_snapping": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M3 2H8V9C8 11.21 9.79 13 12 13C14.21 13 16 11.21 16 9V2H21V9C21 13.97 16.97 18 12 18C7.03 18 3 13.97 3 9V2ZM5 4V6H7V4H5ZM17 4V6H19V4H17ZM10 20H14V23H10V20Z"/>
</svg>''',

    "tool_pivot": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M11 1H13V5H11V1ZM11 19H13V23H11V19ZM1 11H5V13H1V11ZM19 11H23V13H19V11ZM12 6C8.69 6 6 8.69 6 12C6 15.31 8.69 18 12 18C15.31 18 18 15.31 18 12C18 8.69 15.31 6 12 6ZM12 9C13.66 9 15 10.34 15 12C15 13.66 13.66 15 12 15C10.34 15 9 13.66 9 12C9 10.34 10.34 9 12 9Z"/>
</svg>''',

    "tool_camera_pilot": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M12 2C6.48 2 2 6.48 2 12C2 17.52 6.48 22 12 22C17.52 22 22 17.52 22 12C22 6.48 17.52 2 12 2ZM12 4C16.42 4 20 7.58 20 12C20 13.92 19.32 15.68 18.18 17.06L15.66 14.54C15.88 13.78 16 12.91 16 12C16 9.79 14.21 8 12 8C11.09 8 10.22 8.12 9.46 8.34L6.94 5.82C8.32 4.68 10.08 4 12 4ZM4 12C4 10.08 4.68 8.32 5.82 6.94L8.34 9.46C8.12 10.22 8 11.09 8 12C8 14.21 9.79 16 12 16C12.91 16 13.78 15.88 14.54 15.66L17.06 18.18C15.68 19.32 13.92 20 12 20C7.58 20 4 16.42 4 12ZM12 10C10.9 10 10 10.9 10 12C10 13.1 10.9 14 12 14C13.1 14 14 13.1 14 12C14 10.9 13.1 10 12 10Z"/>
</svg>''',

    # Lighting Types
    "light_directional": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M11 1H13V4H11V1ZM11 20H13V23H11V20ZM1 11H4V13H1V11ZM20 11H23V13H20V11ZM4.22 4.22L6.34 6.34L4.93 7.76L2.81 5.64L4.22 4.22ZM17.66 17.66L19.78 19.78L18.36 21.19L16.24 19.07L17.66 17.66ZM4.22 19.78L2.81 18.36L4.93 16.24L6.34 17.66L4.22 19.78ZM19.78 4.22L21.19 5.64L19.07 7.76L17.66 6.34L19.78 4.22ZM12 7C9.24 7 7 9.24 7 12C7 14.76 9.24 17 12 17C14.76 17 17 14.76 17 12C17 9.24 14.76 7 12 7Z"/>
</svg>''',

    "light_point": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M9 21H15V19H9V21ZM12 2C8.13 2 5 5.13 5 9C5 11.38 6.19 13.47 8 14.74V17C8 17.55 8.45 18 9 18H15C15.55 18 16 17.55 16 17V14.74C17.81 13.47 19 11.38 19 9C19 5.13 15.87 2 12 2ZM10 16V14H14V16H10ZM14.85 12.55L14 13.14V9H10V13.14L9.15 12.55C7.81 11.61 7 10.37 7 9C7 6.24 9.24 4 12 4C14.76 4 17 6.24 17 9C17 10.37 16.19 11.61 14.85 12.55Z"/>
</svg>''',

    "light_spot": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M12 2C10.9 2 10 2.9 10 4V6H7L4 16H20L17 6H14V4C14 2.9 13.1 2 12 2ZM6.4 14L8.8 8H15.2L17.6 14H6.4ZM2 19H22V22H2V19Z"/>
</svg>''',

    "light_area": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M2 4H22V17H2V4ZM4 6V15H20V6H4ZM11 18H13V21H16V23H8V21H11V18ZM6 8H10V10H6V8ZM14 8H18V10H14V8ZM6 11H10V13H6V11ZM14 11H18V13H14V11Z"/>
</svg>''',

    "light_hdri": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M12 2C6.48 2 2 6.48 2 12C2 17.52 6.48 22 12 22C17.52 22 22 17.52 22 12C22 6.48 17.52 2 12 2ZM12 4C13.88 4 15.5 7.58 15.86 11H8.14C8.5 7.58 10.12 4 12 4ZM4.14 11C4.58 7.9 6.74 5.38 9.7 4.41C8.61 6.25 7.93 8.5 7.72 11H4.14ZM4.14 13H7.72C7.93 15.5 8.61 17.75 9.7 19.59C6.74 18.62 4.58 16.1 4.14 13ZM12 20C10.12 20 8.5 16.42 8.14 13H15.86C15.5 16.42 13.88 20 12 20ZM14.3 19.59C15.39 17.75 16.07 15.5 16.28 13H19.86C19.42 16.1 17.26 18.62 14.3 19.59ZM16.28 11C16.07 8.5 15.39 6.25 14.3 4.41C17.26 5.38 19.42 7.9 19.86 11H16.28Z"/>
</svg>''',

    # Cameras
    "camera_actor": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M2 6H5.5L7 3H13L14.5 6H18C19.1 6 20 6.9 20 8V18C20 19.1 19.1 20 18 20H2C0.9 20 0 19.1 0 18V8C0 6.9 0.9 6 2 6ZM10 18C13.87 18 17 14.87 17 11C17 7.13 13.87 4 10 4C6.13 4 3 7.13 3 11C3 14.87 6.13 18 10 18ZM10 6.5C12.49 6.5 14.5 8.51 14.5 11C14.5 13.49 12.49 15.5 10 15.5C7.51 15.5 5.5 13.49 5.5 11C5.5 8.51 7.51 6.5 10 6.5ZM21 8.5L24 6V16L21 13.5V8.5Z"/>
</svg>''',

    "camera_aperture": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M12 2C6.48 2 2 6.48 2 12C2 17.52 6.48 22 12 22C17.52 22 22 17.52 22 12C22 6.48 17.52 2 12 2ZM12 4C14.8 4 17.3 5.35 18.9 7.42L12.56 11.08L14.06 8.48L11.75 4.48C11.83 4.48 11.92 4.47 12 4ZM9.76 4.9L12.07 8.9L9.76 10.23L6.15 6.62C7.15 5.72 8.38 5.13 9.76 4.9ZM4.78 8.16L8.39 11.77L6.89 14.37L4.09 12.75C4.03 12.5 4 12.25 4 12C4 10.6 4.4 9.29 4.78 8.16ZM4.84 15.48L7.64 17.1L9.14 14.5L12.75 18.11C11.37 18.71 9.87 18.94 8.41 18.66C6.91 17.94 5.67 16.83 4.84 15.48ZM14.15 18.92L11.84 14.92L14.15 13.59L17.76 17.2C16.69 18.15 15.46 18.77 14.15 18.92ZM19.22 15.84L15.61 12.23L17.11 9.63L19.91 11.25C19.97 11.5 20 11.75 20 12C20 13.4 19.6 14.71 19.22 15.84Z"/>
</svg>''',

    "camera_safe_frame": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M2 3H8V6H5V8H2V3ZM16 3H22V8H19V6H16V3ZM2 16H5V18H8V21H2V16ZM19 18H16V21H22V16H19V18ZM4 9H20V15H4V9ZM6 11V13H18V11H6Z"/>
</svg>''',

    # Animation Transport
    "anim_play": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M6 3.5L20 12L6 20.5V3.5Z"/>
</svg>''',

    "anim_pause": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M5 3H9V21H5V3ZM15 3H19V21H15V3Z"/>
</svg>''',

    "anim_step_forward": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M4 4L15 12L4 20V4ZM16 4H20V20H16V4Z"/>
</svg>''',

    "anim_step_back": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M20 4L9 12L20 20V4ZM4 4H8V20H4V4Z"/>
</svg>''',

    "anim_jump_start": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M21 4L11 12L21 20V4ZM11 4L1 12L11 20V4ZM1 4H4V20H1V4Z"/>
</svg>''',

    "anim_jump_end": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M3 4L13 12L3 20V4ZM13 4L23 12L13 20V4ZM20 4H23V20H20V4Z"/>
</svg>''',

    "anim_loop": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M19 8L15 4V7H8C5.24 7 3 9.24 3 12H6C6 10.9 6.9 10 8 10H15V13L19 8ZM5 16L9 20V17H16C18.76 17 21 14.76 21 12H18C18 13.1 17.1 14 16 14H9V11L5 16Z"/>
</svg>''',

    "anim_keyframe": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M12 2L2 12L12 22L22 12L12 2ZM12 6.5L17.5 12L12 17.5L6.5 12L12 6.5Z"/>
</svg>''',

    "anim_autokey": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M12 2C6.48 2 2 6.48 2 12C2 17.52 6.48 22 12 22C17.52 22 22 17.52 22 12C22 6.48 17.52 2 12 2ZM12 5C15.87 5 19 8.13 19 12C19 15.87 15.87 19 12 19C8.13 19 5 15.87 5 12C5 8.13 8.13 5 12 5ZM12 8C9.79 8 8 9.79 8 12C8 14.21 9.79 16 12 16C14.21 16 16 14.21 16 12C16 9.79 14.21 8 12 8Z"/>
</svg>''',

    # Scene Outliner & Toggles
    "outliner_mesh": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M12 2L2 7V17L12 22L22 17V7L12 2ZM12 4.3L19.5 8.2L12 12.1L4.5 8.2L12 4.3ZM4 10.1L11 13.7V19.7L4 16.1V10.1ZM13 19.7V13.7L20 10.1V16.1L13 19.7Z"/>
</svg>''',

    "outliner_light": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M11 1H13V5H11V1ZM4 12H1V10H4V12ZM23 10V12H20V10H23ZM7.05 4.93L5.64 3.51L4.22 4.93L5.64 6.34L7.05 4.93ZM18.36 6.34L19.78 4.93L18.36 3.51L16.95 4.93L18.36 6.34ZM12 6C8.69 6 6 8.69 6 12C6 14.54 7.58 16.71 9.8 17.56V20H14.2V17.56C16.42 16.71 18 14.54 18 12C18 8.69 15.31 6 12 6ZM10 21H14V23H10V21Z"/>
</svg>''',

    "outliner_camera": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M3 4H15V8L21 4V18L15 14V18H3V4ZM5 6V16H13V6H5ZM16 11.5L19 13.5V8.5L16 10.5V11.5Z"/>
</svg>''',

    "outliner_collection": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M2 3H9L11 6H22V20H2V3ZM4 8V18H20V8H4ZM6 10H18V12H6V10Z"/>
</svg>''',

    "toggle_eye_on": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M12 4.5C7 4.5 2.73 7.61 1 12C2.73 16.39 7 19.5 12 19.5C17 19.5 21.27 16.39 23 12C21.27 7.61 17 4.5 12 4.5ZM12 17C9.24 17 7 14.76 7 12C7 9.24 9.24 7 12 7C14.76 7 17 9.24 17 12C17 14.76 14.76 17 12 17ZM12 9.5C10.62 9.5 9.5 10.62 9.5 12C9.5 13.38 10.62 14.5 12 14.5C13.38 14.5 14.5 13.38 14.5 12C14.5 10.62 13.38 9.5 12 9.5Z"/>
</svg>''',

    "toggle_eye_off": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M2.81 2.81L1.39 4.22L4.78 7.61C3.33 8.79 2.1 10.27 1 12C2.73 16.39 7 19.5 12 19.5C14.12 19.5 16.11 18.89 17.81 17.81L19.78 19.78L21.19 18.36L2.81 2.81ZM12 17C9.24 17 7 14.76 7 12C7 11.23 7.18 10.51 7.49 9.87L9.46 11.84C9.48 11.89 9.5 11.94 9.5 12C9.5 13.38 10.62 14.5 12 14.5C12.06 14.5 12.11 14.48 12.16 14.46L14.13 16.43C13.49 16.79 12.77 17 12 17ZM12 4.5C14.07 4.5 16.03 5.08 17.75 6.11L16.27 7.59C15 6.88 13.55 6.5 12 6.5C9.24 6.5 7 8.74 7 11.5C7 11.82 7.04 12.13 7.1 12.43L5.42 10.75C6.18 8.18 8.84 5.95 12 4.5ZM23 12C22.18 9.94 20.73 8.19 18.9 6.94L20.44 5.4C22.56 7.12 24.16 9.38 25 12C23.27 16.39 19 19.5 14 19.5L15.65 17.85C19.23 16.92 22.02 14.58 23 12Z"/>
</svg>''',

    "toggle_lock_on": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M18 8H17V6C17 3.24 14.76 1 12 1C9.24 1 7 3.24 7 6V8H6C4.9 8 4 8.9 4 10V20C4 21.1 4.9 22 6 22H18C19.1 22 20 21.1 20 20V10C20 8.9 19.1 8 18 8ZM12 17C10.9 17 10 16.1 10 15C10 13.9 10.9 13 12 13C13.1 13 14 13.9 14 15C14 16.1 13.1 17 12 17ZM15 8H9V6C9 4.34 10.34 3 12 3C13.66 3 15 4.34 15 6V8Z"/>
</svg>''',

    "toggle_lock_off": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M18 8H17V6C17 3.24 14.76 1 12 1C9.24 1 7 3.24 7 6H9C9 4.34 10.34 3 12 3C13.66 3 15 4.34 15 6V8H6C4.9 8 4 8.9 4 10V20C4 21.1 4.9 22 6 22H18C19.1 22 20 21.1 20 20V10C20 8.9 19.1 8 18 8ZM12 17C10.9 17 10 16.1 10 15C10 13.9 10.9 13 12 13C13.1 13 14 13.9 14 15C14 16.1 13.1 17 12 17Z"/>
</svg>''',

    "file_save": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M19 2H5C3.9 2 3 2.9 3 4V20C3 21.1 3.9 22 5 22H19C20.1 22 21 21.1 21 20V6L19 2ZM12 19C10.34 19 9 17.66 9 16C9 14.34 10.34 13 12 13C13.66 13 15 14.34 15 16C15 17.66 13.66 19 12 19ZM6 4H15V8H6V4Z"/>
</svg>''',

    "action_render": '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
  <path d="M19 4L21 8H18L16 4H14L16 8H13L11 4H9L11 8H8L6 4H4C2.9 4 2 4.9 2 6V18C2 19.1 2.9 20 4 20H20C21.1 20 22 19.1 22 18V4H19ZM10 16.5V9.5L16 13L10 16.5Z"/>
</svg>'''
}

def main():
    icons_dir = r"Y:\Zelyn Ecosystem\MedusaPlatinum\resources\icons"
    os.makedirs(icons_dir, exist_ok=True)
    
    # Save individual SVGs
    for name, svg_content in ICONS.items():
        filepath = os.path.join(icons_dir, f"{name}.svg")
        with open(filepath, "w", encoding="utf-8") as f:
            f.write(svg_content.strip())
        print(f"Generated: {filepath}")

    # Generate HTML Showcase
    html_path = os.path.join(icons_dir, "icon_showcase.html")
    html_content = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>Medusa Platinum — Custom Heavy DCC Icon Showcase</title>
<style>
  body {
    background-color: #161618;
    color: #e0e0e4;
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
    margin: 0;
    padding: 32px;
  }
  h1 { font-size: 20px; font-weight: 600; color: #ffffff; margin-bottom: 8px; }
  p.subtitle { color: #888890; font-size: 13px; margin-bottom: 24px; }
  .grid {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(180px, 1fr));
    gap: 16px;
  }
  .card {
    background: #1f1f23;
    border: 1px solid #2e2e34;
    border-radius: 6px;
    padding: 16px;
    display: flex;
    flex-direction: column;
    align-items: center;
    transition: border-color 0.15s ease;
  }
  .card:hover {
    border-color: #555560;
  }
  .icon-preview {
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 12px;
    margin-bottom: 12px;
    height: 48px;
  }
  .icon-preview svg {
    color: #cccccc;
    display: block;
  }
  .icon-preview svg.sz-16 { width: 16px; height: 16px; }
  .icon-preview svg.sz-24 { width: 24px; height: 24px; color: #ffffff; }
  .icon-preview svg.sz-32 { width: 32px; height: 32px; }
  .icon-name {
    font-size: 11px;
    font-family: 'JetBrains Mono', Consolas, monospace;
    color: #a0a0ab;
    text-align: center;
    word-break: break-all;
  }
  .badge {
    font-size: 9px;
    background: #2a2a30;
    color: #00adb5;
    padding: 2px 6px;
    border-radius: 3px;
    margin-top: 6px;
  }
</style>
</head>
<body>
  <h1>Medusa Platinum — Heavyweight Custom DCC Icon Set</h1>
  <p class="subtitle">Engineered for 3D staging, DXR production lookdev, and cinematic animation. Heavy silhouettes, zero web-line fragility.</p>
  <div class="grid">
"""
    for name, svg_content in ICONS.items():
        # Inject classes
        svg16 = svg_content.replace('<svg ', '<svg class="sz-16" ')
        svg24 = svg_content.replace('<svg ', '<svg class="sz-24" ')
        svg32 = svg_content.replace('<svg ', '<svg class="sz-32" ')
        
        category = name.split('_')[0].upper()
        html_content += f"""    <div class="card">
      <div class="icon-preview">
        {svg16}
        {svg24}
        {svg32}
      </div>
      <div class="icon-name">{name}</div>
      <div class="badge">{category}</div>
    </div>\n"""

    html_content += """  </div>
</body>
</html>"""
    with open(html_path, "w", encoding="utf-8") as f:
        f.write(html_content)
    print(f"Generated Showcase: {html_path}")

    # Generate C++ Header for Direct2D compilation
    include_dir = r"Y:\Zelyn Ecosystem\MedusaPlatinum\include\medusa"
    os.makedirs(include_dir, exist_ok=True)
    header_path = os.path.join(include_dir, "medusa_icons.hpp")
    with open(header_path, "w", encoding="utf-8") as f:
        f.write("""// Medusa Platinum — High-Performance Native DCC Custom Icon Registry
// Automatically generated for Direct2D / DirectWrite UI rendering.
#pragma once

#include <string_view>
#include <unordered_map>

namespace medusa::ui::icons {

enum class IconId {
""")
        for name in ICONS.keys():
            f.write(f"    {name.title().replace('_', '')},\n")
        f.write("""};

struct IconDefinition {
    const char* id;
    const char* svg_xml;
};

inline const IconDefinition kAllIcons[] = {
""")
        for name, svg_content in ICONS.items():
            escaped_svg = svg_content.replace('"', '\\"').replace('\n', ' ')
            f.write(f'    {{ "{name}", "{escaped_svg}" }},\n')
        f.write("""};

} // namespace medusa::ui::icons
""")
    print(f"Generated C++ Header: {header_path}")

if __name__ == "__main__":
    main()
