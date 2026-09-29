import json
import os
import subprocess
import time
import streamlit as st

st.set_page_config(
    page_title="AgriBot Autonomous Field Terminal",
    page_icon="🤖",
    layout="wide",
    initial_sidebar_state="expanded"
)

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

# Discovered themes
THEME = {
    "H": {"bg": "linear-gradient(135deg, #064e3b 0%, #022c22 100%)", "border": "rgba(16, 185, 129, 0.2)", "icon": "🌱"},
    "W": {"bg": "linear-gradient(135deg, #78350f 0%, #451a03 100%)", "border": "rgba(245, 158, 11, 0.2)", "icon": "🌾"},
    "D": {"bg": "linear-gradient(135deg, #7f1d1d 0%, #450a0a 100%)", "border": "rgba(239, 68, 68, 0.2)", "icon": "🥀"}
}

# Unexplored Fog of War theme
FOG_THEME = {
    "bg": "linear-gradient(135deg, #182026 0%, #111518 100%)",
    "border": "rgba(255, 255, 255, 0.05)",
    "icon": "🌫️"
}

# --- Sidebar Controls ---
with st.sidebar:
    st.title("⚙️️ Telemetry Controls")
    
    speed = st.slider("Animation Delay (seconds)", min_value=0.1, max_value=1.5, value=0.35, step=0.05)
    st.caption("Adjusts pacing for autonomous sweep and queue step execution.")

    st.markdown("---")
    st.subheader("Autonomous Routines")
    
    if st.button("🚀 Auto-Survey Routine (Full Scan)", use_container_width=True):
        survey_steps = []
        for r in range(5):
            cols = range(5) if r % 2 == 0 else range(4, -1, -1)
            for c in cols:
                survey_steps.append((r, c))

        cur_x, cur_y = st.session_state.state["x"], st.session_state.state["y"]
        
        # Enqueue trajectory
        for target_r, target_c in survey_steps:
            while cur_x < target_r:
                call_core("queue_add", type="move", dir="down")
                cur_x += 1
            while cur_x > target_r:
                call_core("queue_add", type="move", dir="up")
                cur_x -= 1
            while cur_y < target_c:
                call_core("queue_add", type="move", dir="right")
                cur_y += 1
            while cur_y > target_c:
                call_core("queue_add", type="move", dir="left")
                cur_y -= 1
            call_core("queue_add", type="inspect")

        st.session_state.state = call_core("state")
        st.rerun()

    st.markdown("---")
    st.subheader("Architecture")
    st.markdown("""
    - **Fog of War**: Hidden quadrants
    - **Polymorphism**: `ActionCommand` abstract base
    - **Encapsulation**: Private robot coordinates & energy
    - **Undo / Redo**: LIFO `std::stack`
    - **Mission Queue**: FIFO `std::queue`
    """)

# --- Top Banner ---
st.title("🛰️ AgriBot Autonomous Tactical Grid")
banner_placeholder = st.empty()

col_map, col_ops = st.columns([1.5, 1], gap="large")

# Render Matrix Helper
def render_matrix(target_container, current_state):
    with target_container.container():
        st.subheader("Field Topology Matrix")
        for r in range(5):
            row_cols = st.columns(5)
            for c in range(5):
                ch = current_state["grid"][r][c]
                is_bot = (current_state["x"] == r and current_state["y"] == c)
                is_revealed = current_state["discovered"][r][c]

                meta = THEME.get(ch, THEME["H"]) if is_revealed else FOG_THEME
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
                    if is_revealed and ch == "W":
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

# --- Left Column: Map & Controls ---
with col_map:
    matrix_placeholder = st.empty()
    render_matrix(matrix_placeholder, st.session_state.state)

    st.markdown("<br>", unsafe_allow_html=True)
    k1, k2, k3, k4, k5 = st.columns(5)
    k1.markdown("🤖 **Rover**")
    k2.markdown("🌫️ **Fog/Hidden**")
    k3.markdown("🌱 **Healthy**")
    k4.markdown("🌾 **Weed**")
    k5.markdown("🥀 **Blight**")

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
    if c5.button("⬇️️ South", use_container_width=True):
        st.session_state.state = call_core("move", dir="down")
        st.rerun()
    if c6.button("➡️ East", use_container_width=True):
        st.session_state.state = call_core("move", dir="right")
        st.rerun()

    a1, a2 = st.columns(2)
    if a1.button("🔬 Multispectral Scan (-2%)", use_container_width=True):
        st.session_state.state = call_core("inspect")
        st.rerun()
    if a2.button("💦 Precision Spray (-8%)", use_container_width=True):
        st.session_state.state = call_core("spray")
        st.rerun()

    u1, u2, u3 = st.columns(3)
    if u1.button("⏪ Undo Stack", use_container_width=True):
        st.session_state.state = call_core("undo")
        st.rerun()
    if u2.button("⏩ Redo Stack", use_container_width=True):
        st.session_state.state = call_core("redo")
        st.rerun()
    if u3.button("🎲 Reset Matrix", use_container_width=True):
        st.session_state.state = call_core("reset")
        st.rerun()

# --- Right Column: Diagnostics & Pipeline ---
with col_ops:
    st.subheader("System Diagnostics")

    batt = st.session_state.state["battery"]
    discovered_count = sum(sum(1 for cell in row if cell) for row in st.session_state.state["discovered"])
    
    d1, d2, d3 = st.columns(3)
    d1.metric("Reserve Power", f"{batt}%")
    d2.metric("Grid Vector", f"[{st.session_state.state['x']}, {st.session_state.state['y']}]")
    d3.metric("Scouted", f"{discovered_count}/25")
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
    if qb3.button("▶️ Run Queue (Animated)", use_container_width=True):
        # Step-by-step animated execution
        while st.session_state.state["queue"]:
            st.session_state.state = call_core("queue_next")
            render_matrix(matrix_placeholder, st.session_state.state)
            banner_placeholder.markdown(f"""
            <div class="status-banner">
                <strong>TELEMETRY BUS:</strong> {st.session_state.state.get('msg', 'Processing queue...')}
            </div>
            """, unsafe_allow_html=True)
            time.sleep(speed)
        st.rerun()

    with st.expander("Active Pipeline Tasks", expanded=True):
        if st.session_state.state["queue"]:
            for idx, q_cmd in enumerate(st.session_state.state["queue"], 1):
                st.code(f"PIPE #{idx}: {q_cmd}")
        else:
            st.caption("No instructions in pipeline.")

    tab_u, tab_r, tab_h = st.tabs(["LIFO Undo Stack", "LIFO Redo Stack", "Vector Audit"])
    with tab_u:
        if st.session_state.state["undo"]:
            for item in reversed(st.session_state.state["undo"]):
                st.text(f"⮌ {item}")
        else:
            st.caption("Stack empty.")

    with tab_r:
        if st.session_state.state["redo"]:
            for item in reversed(st.session_state.state["redo"]):
                st.text(f"⮎ {item}")
        else:
            st.caption("Stack empty.")

    with tab_h:
        if st.session_state.state["history"]:
            for h in reversed(st.session_state.state["history"][-8:]):
                st.caption(f"• {h}")
        else:
            st.caption("No history recorded.")

# Render final status banner
banner_placeholder.markdown(f"""
<div class="status-banner">
    <strong>TELEMETRY BUS:</strong> {st.session_state.state.get('msg', 'System active.')}
</div>
""", unsafe_allow_html=True)
