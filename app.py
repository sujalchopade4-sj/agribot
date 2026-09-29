import json
import os
import subprocess
import streamlit as st

st.set_page_config(
    page_title="AgriBot Autonomous Field Terminal",
    page_icon="🤖",
    layout="wide",
    initial_sidebar_state="expanded"
)

# High-tech Cyber Agritech UI styling and keyframe animations
st.markdown("""
<style>
    @import url('https://fonts.googleapis.com/css2?family=JetBrains+Mono:wght@400;600;700&display=swap');
    
    html, body, [class*="css"] {
        font-family: 'JetBrains Mono', monospace;
    }

    @keyframes radar-pulse {
        0% {
            box-shadow: 0 0 0 0 rgba(74, 222, 128, 0.7), inset 0 0 10px rgba(74, 222, 128, 0.5);
            border-color: #4ade80;
        }
        50% {
            box-shadow: 0 0 0 12px rgba(74, 222, 128, 0), inset 0 0 20px rgba(74, 222, 128, 0.8);
            border-color: #86efac;
        }
        100% {
            box-shadow: 0 0 0 0 rgba(74, 222, 128, 0), inset 0 0 10px rgba(74, 222, 128, 0.5);
            border-color: #4ade80;
        }
    }

    @keyframes weed-drift {
        0% { transform: translateY(0px) rotate(0deg); }
        50% { transform: translateY(-3px) rotate(2deg); }
        100% { transform: translateY(0px) rotate(0deg); }
    }

    .agri-card {
        background: rgba(18, 24, 27, 0.85);
        border: 1px solid rgba(74, 222, 128, 0.15);
        backdrop-filter: blur(8px);
        border-radius: 12px;
        padding: 16px;
        margin-bottom: 12px;
        box-shadow: 0 8px 32px 0 rgba(0, 0, 0, 0.37);
    }

    .cell-box {
        height: 72px;
        border-radius: 10px;
        display: flex;
        align-items: center;
        justify-content: center;
        position: relative;
        font-size: 26px;
        transition: all 0.25s cubic-bezier(0.4, 0, 0.2, 1);
        cursor: default;
    }

    .cell-box:hover {
        transform: translateY(-4px) scale(1.04);
        box-shadow: 0 10px 20px rgba(0,0,0,0.5);
        z-index: 10;
    }

    .robot-cell {
        animation: radar-pulse 2s infinite ease-out;
        z-index: 5;
    }

    .weed-cell {
        animation: weed-drift 4s infinite ease-in-out;
    }

    .coord-badge {
        position: absolute;
        bottom: 3px;
        right: 5px;
        font-size: 9px;
        opacity: 0.45;
        letter-spacing: -0.5px;
    }

    .status-banner {
        border-left: 4px solid #4ade80;
        background: rgba(74, 222, 128, 0.08);
        padding: 10px 16px;
        border-radius: 4px;
        font-size: 13px;
        margin-bottom: 16px;
    }

    .stButton>button {
        border-radius: 8px;
        font-weight: 600;
        letter-spacing: 0.5px;
        transition: all 0.2s ease;
    }

    .stButton>button:hover {
        box-shadow: 0 0 10px rgba(74, 222, 128, 0.3);
        transform: translateY(-1px);
    }
</style>
""", unsafe_allow_html=True)

HERE = os.path.dirname(os.path.abspath(__file__))
STATE_FILE = os.path.join(HERE, "agribot_state.txt")
EXE_NAME = "agribot_core.exe" if os.name == "nt" else "agribot_core"
EXE_PATH = os.path.join(HERE, EXE_NAME)
CPP_PATH = os.path.join(HERE, "agribot_core.cpp")

def ensure_exe_built():
    if os.path.exists(EXE_PATH):
        return True
    if not os.path.exists(CPP_PATH):
        return False
    try:
        res = subprocess.run(
            ["g++", "-std=c++17", CPP_PATH, "-o", EXE_PATH],
            capture_output=True, text=True, timeout=60
        )
        if res.returncode != 0:
            st.error(f"Compile Error:\n{res.stderr}")
            return False
        if os.name != "nt":
            os.chmod(EXE_PATH, 0o755)
        return True
    except Exception as e:
        st.error(f"Compiler missing or build failure: {e}")
        return False

ensure_exe_built()

def call_core(action, **kwargs):
    if not os.path.exists(EXE_PATH):
        st.error("Executable missing. Make sure g++ compiler is available.")
        st.stop()
    args = [EXE_PATH, STATE_FILE, action] + [f"{k}={v}" for k, v in kwargs.items()]
    try:
        res = subprocess.run(args, capture_output=True, text=True, timeout=5)
    except Exception as e:
        st.error(f"Process Error: {e}")
        st.stop()

    if not res.stdout.strip():
        st.error(f"Core output null. Stderr:\n{res.stderr}")
        st.stop()

    try:
        return json.loads(res.stdout.strip())
    except json.JSONDecodeError:
        st.error(f"JSON Parse failure:\n{res.stdout}")
        st.stop()

if "state" not in st.session_state:
    st.session_state.state = call_core("state")

state = st.session_state.state

# Theme configuration
THEME = {
    "H": {"bg": "linear-gradient(135deg, #064e3b 0%, #022c22 100%)", "border": "rgba(16, 185, 129, 0.2)", "icon": "🌱"},
    "W": {"bg": "linear-gradient(135deg, #78350f 0%, #451a03 100%)", "border": "rgba(245, 158, 11, 0.2)", "icon": "🌾"},
    "D": {"bg": "linear-gradient(135deg, #7f1d1d 0%, #450a0a 100%)", "border": "rgba(239, 68, 68, 0.2)", "icon": "🥀"}
}

# --- Sidebar: Export Project Files ---
with st.sidebar:
    st.title("📦 Project Packages")
    st.caption("Download the complete source files to run anywhere.")
    
    # Read files for instant download
    if os.path.exists(CPP_PATH):
        with open(CPP_PATH, "r") as f:
            st.download_button("💾 Download agribot_core.cpp", f.read(), "agribot_core.cpp", "text/x-c++src", use_container_width=True)
    
    with open(__file__, "r") as f:
        st.download_button("💾 Download app.py", f.read(), "app.py", "text/x-python", use_container_width=True)
        
    st.download_button("💾 Download packages.txt", "g++\n", "packages.txt", "text/plain", use_container_width=True)
    
    st.markdown("---")
    st.subheader("Architecture")
    st.markdown("""
    - **Polymorphism**: `ActionCommand` abstract base
    - **Encapsulation**: Private robot coordinates & energy
    - **Undo / Redo**: LIFO `std::stack`
    - **Mission Queue**: FIFO `std::queue`
    - **Map Randomizer**: `std::shuffle`
    """)

# --- Top Banner ---
st.title("🛰️ AgriBot Autonomous Tactical Grid")
st.markdown(f"""
<div class="status-banner">
    <strong>TELEMETRY BUS:</strong> {state.get('msg', 'System active.')}
</div>
""", unsafe_allow_html=True)

col_map, col_ops = st.columns([1.5, 1], gap="large")

# --- Left Column: Animated Grid & Manual D-Pad ---
with col_map:
    st.subheader("Field Topology Matrix")

    for r in range(5):
        row_cols = st.columns(5)
        for c in range(5):
            ch = state["grid"][r][c]
            meta = THEME.get(ch, THEME["H"])
            is_bot = (state["x"] == r and state["y"] == c)

            cell_class = "cell-box"
            if is_bot:
                cell_class += " robot-cell"
                icon = "🤖"
                border = "2px solid #4ade80"
                bg = "linear-gradient(135deg, #14532d 0%, #052e16 100%)"
            else:
                icon = meta["icon"]
                border = f"1px solid {meta['border']}"
                bg = meta["bg"]
                if ch == "W":
                    cell_class += " weed-cell"

            row_cols[c].markdown(
                f"""
                <div class="{cell_class}" style="background: {bg}; border: {border};">
                    <span>{icon}</span>
                    <span class="coord-badge">{r},{c}</span>
                </div>
                """,
                unsafe_allow_html=True
            )

    st.markdown("<br>", unsafe_allow_html=True)
    k1, k2, k3, k4 = st.columns(4)
    k1.markdown("🤖 **Active Rover**")
    k2.markdown("🌱 **Healthy Crop**")
    k3.markdown("🌾 **Weed Colony**")
    k4.markdown("🥀 **Fungal Blight**")

    st.markdown("---")
    st.subheader("Manual Teleoperation Control")

    c1, c2, c3 = st.columns(3)
    if c2.button("⬆️ North", use_container_width=True):
        st.session_state.state = call_core("move", dir="up")
        st.rerun()

    c4, c5, c6 = st.columns(3)
    if c4.button("⬅️ West", use_container_width=True):
        st.session_state.state = call_core("move", dir="left")
        st.rerun()
    if c5.button("⬇️ South", use_container_width=True):
        st.session_state.state = call_core("move", dir="down")
        st.rerun()
    if c6.button("➡️ East", use_container_width=True):
        st.session_state.state = call_core("move", dir="right")
        st.rerun()

    a1, a2 = st.columns(2)
    if a1.button("🔬 Multispectral Scan (-2%)", use_container_width=True):
        st.session_state.state = call_core("inspect")
        st.rerun()
    if a2.button("💦 Chemical Neutralize (-8%)", use_container_width=True):
        st.session_state.state = call_core("spray")
        st.rerun()

    u1, u2, u3 = st.columns(3)
    if u1.button("⏪ Undo Stack", use_container_width=True):
        st.session_state.state = call_core("undo")
        st.rerun()
    if u2.button("⏩ Redo Stack", use_container_width=True):
        st.session_state.state = call_core("redo")
        st.rerun()
    if u3.button("🎲 Randomize Field", use_container_width=True):
        st.session_state.state = call_core("reset")
        st.rerun()

# --- Right Column: Diagnostics, Queues & Stacks ---
with col_ops:
    st.subheader("System Diagnostics")

    batt = state["battery"]
    d1, d2 = st.columns(2)
    d1.metric("Reserve Power", f"{batt}%")
    d2.metric("Grid Vector", f"[{state['x']}, {state['y']}]")
    st.progress(batt / 100)

    st.markdown("---")
    st.subheader("FIFO Mission Dispatcher (`std::queue`)")

    col_sel1, col_sel2 = st.columns(2)
    with col_sel1:
        q_act = st.selectbox("Action Type", ["move", "spray", "inspect"])
    with col_sel2:
        q_dir = st.selectbox("Trajectory", ["up", "down", "left", "right"]) if q_act == "move" else None

    qb1, qb2, qb3 = st.columns(3)
    if qb1.button("Push Queue", use_container_width=True):
        payload = {"type": q_act}
        if q_dir:
            payload["dir"] = q_dir
        st.session_state.state = call_core("queue_add", **payload)
        st.rerun()
    if qb2.button("Step Queue", use_container_width=True):
        st.session_state.state = call_core("queue_next")
        st.rerun()
    if qb3.button("Flush All", use_container_width=True):
        st.session_state.state = call_core("queue_all")
        st.rerun()

    with st.expander("Active Pipeline Tasks", expanded=True):
        if state["queue"]:
            for idx, q_cmd in enumerate(state["queue"], 1):
                st.code(f"PIPE #{idx}: {q_cmd}")
        else:
            st.caption("No instructions in pipeline.")

    tab_u, tab_r, tab_h = st.tabs(["LIFO Undo Stack", "LIFO Redo Stack", "Vector Audit"])
    with tab_u:
        if state["undo"]:
            for item in reversed(state["undo"]):
                st.text(f"⮌ {item}")
        else:
            st.caption("Stack empty.")

    with tab_r:
        if state["redo"]:
            for item in reversed(state["redo"]):
                st.text(f"⮎ {item}")
        else:
            st.caption("Stack empty.")

    with tab_h:
        if state["history"]:
            for h in reversed(state["history"][-8:]):
                st.caption(f"• {h}")
        else:
            st.caption("No history recorded.")
