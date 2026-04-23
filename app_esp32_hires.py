import streamlit as st
import cv2
import numpy as np
import time
from datetime import datetime
from pathlib import Path
import glob
from captura_esp32_hires import ESP32HiRes

st.set_page_config(page_title="ESP32-CAM HiRes", layout="wide", initial_sidebar_state="collapsed")

if 'esp32' not in st.session_state:
    st.session_state.esp32 = None
    st.session_state.conectado = False
    st.session_state.stream_activo = False
    st.session_state.last_frame = None

if 'fotos' not in st.session_state:
    st.session_state.fotos = []

GALERIA_DIR = Path("/Volumes/USB/Proyecto/galeria")
GALERIA_DIR.mkdir(exist_ok=True)

puertos_posibles = glob.glob("/dev/cu.usbserial*")
puerto_defecto = puertos_posibles[0] if puertos_posibles else "/dev/cu.usbserial-1130"

st.title("📸 Sistema ESP32-CAM")

col_video, col_filtros = st.columns([7, 3])

with col_filtros:
    st.subheader("⚙️ Control de Cámara")
    
    col_c1, col_c2 = st.columns(2)
    with col_c1:
        if st.button("🔌 Conectar", width='stretch', type="primary"):
            esp32 = ESP32HiRes(puerto_defecto, 115200)
            if esp32.conectar():
                st.session_state.esp32 = esp32
                st.session_state.conectado = True
                st.session_state.esp32.iniciar_stream()
                st.session_state.stream_activo = True
                st.success("✅ Conectado y Stream Iniciado")
            else:
                st.error("❌ Falló conexión")
    with col_c2:
        if st.button("⏹️ Detener", width='stretch'):
            if st.session_state.esp32:
                st.session_state.esp32.detener_stream()
                st.session_state.esp32.desconectar()
                st.session_state.conectado = False
                st.session_state.stream_activo = False
    
    st.divider()
    
    st.subheader("🎨 Filtros")
    
    filtros = [
        "original", "invert", "grayscale", "red_tint", "green_tint", "blue_tint", "sepia", 
        "contraste", "soleado", "test_patron", "lineas", "profundidad"
    ]
    filtro_seleccionado = st.selectbox("Elegir filtro:", filtros)
    
    if st.button(f"📷 Emitir Captura '{filtro_seleccionado.title()}'", width='stretch', type="primary"):
        if st.session_state.conectado and st.session_state.esp32:
            st.toast("Deteniendo stream temporalmente...")
            st.session_state.stream_activo = False
            st.session_state.esp32.detener_stream()
            time.sleep(1.5) 
            
            if st.session_state.esp32.ser:
                st.session_state.esp32.ser.reset_input_buffer()
                st.session_state.esp32.ser.reset_output_buffer()
            
            with st.spinner("Procesando captura..."):
                frame_captura = st.session_state.esp32.capturar_con_filtro(filtro_seleccionado)
                
                if frame_captura is not None:
                    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                    nombre_archivo = f"{filtro_seleccionado}_{timestamp}.jpg"
                    ruta = GALERIA_DIR / nombre_archivo
                    cv2.imwrite(str(ruta), frame_captura)
                    
                    st.session_state.fotos.append({
                        'ruta': str(ruta),
                        'filtro': filtro_seleccionado,
                        'timestamp': timestamp
                    })
                    st.success(f"✅ Foto guardada")
                else:
                    st.error("❌ Error de timeout.")
            
            st.toast("Reanudando stream...")
            st.session_state.esp32.iniciar_stream()
            st.session_state.stream_activo = True
        else:
            st.warning("⚠️ Debes conectar primero.")

    st.divider()
    
    st.subheader("🖼️ Galería Reciente")
    if st.session_state.fotos:
        ultima_foto = st.session_state.fotos[-1]
        img_galeria = cv2.imread(ultima_foto['ruta'])
        if img_galeria is not None:
            st.image(cv2.cvtColor(img_galeria, cv2.COLOR_BGR2RGB), caption=f"Última: {ultima_foto['filtro']}", width='stretch')
            
        if st.button("🗑️ Limpiar Historial Visual", width='stretch'):
            st.session_state.fotos = []
            st.rerun()
    else:
        st.info("Ninguna foto capturada.")

with col_video:
    st.subheader("📹 Visor en Vivo")
    stream_placeholder = st.empty()
    
    if not st.session_state.conectado:
        img_blank = np.zeros((480, 640, 3), dtype=np.uint8)
        cv2.putText(img_blank, "CAMARA DESCONECTADA", (120, 240), cv2.FONT_HERSHEY_SIMPLEX, 1, (100,100,100), 2)
        stream_placeholder.image(img_blank, width='stretch')

if st.session_state.conectado and st.session_state.stream_activo:
    fps_time = time.time()
    fps_frames = 0
    
    while st.session_state.stream_activo:
        frame = st.session_state.esp32.obtener_frame(timeout=0.08)
        
        if frame is not None:
            frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            
            fps_frames += 1
            ahora = time.time()
            if ahora - fps_time >= 1.0:
                fps_text = f"FPS: {fps_frames / (ahora - fps_time):.1f}"
                fps_frames = 0
                fps_time = ahora
                cv2.putText(frame_rgb, fps_text, (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
                
            stream_placeholder.image(frame_rgb, width='stretch')
        else:
            time.sleep(0.01)
