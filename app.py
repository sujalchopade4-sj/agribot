import json
import os
import subprocess
import time
import streamlit as st

st.set_page_config(
    page_title="AgriBot Tactical Field Commander",
    page_icon="🚜",
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
            box-shadow: 0 0 0 10px rgba(74, 222, 128, 0), inset 0 0 16px rgba(74, 222, 128, 0.8);
            border-color: #86efac;
        }
        100% {
            box-shadow: 0 0 0 0 rgba(74, 222, 128, 0), inset 0 0 10px rgba(74, 222, 128, 0.5);
            border-color: #4ade80;
        }
    }

    .cell-10 {
        height: 48px;
        border-radius: 6px;
        display: flex;
        align-items: center;
        justify-content: center;
        position: relative;
        font-size: 20px;
        transition: transform 0.15s ease-in-out;
        cursor: default;
    }

    .cell-10:hover {
        transform: scale(1.08);
        z-index: 10;
    }

    .robot-cell {
        animation: radar-pulse 1.8s infinite ease-out;
        z-index: 5;
    }

    .base-station {
        border: 2px dashed #38bdf8 !important;
    }

    .coord-tag {
        position: absolute;
        bottom: 2px;
        right: 3px;
        font-size: 8px;
        opacity: 0.4;
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
    }
</style>
""", unsafe_allow_html=True)

HERE = os.path.dirname(os.path.abspath(__file__))
STATE_FILE = os.path.join(HERE, "agribot_state.txt")
EXE_NAME = "agribot_core.exe" if os.name == "nt" else "agribot_core"
EXE_PATH = os.path.join(HERE, EXE_NAME)
CPP_PATH = os.path.join(HERE, "agribot_core.cpp")

def ensure_exe_built():
    needs_build = not os.path.exists(EXE_PATH)
    if os.path.exists(CPP_PATH) and os.path.exists(EXE_PATH):
        if os.path.getmtime(CPP_PATH) > os.path.getmtime(EXE_PATH):
            needs_build = True

    if needs_build and os.path.exists(CPP_PATH):
        try:
            res = subprocess.run(
                ["g++", "-std=c++17", CPP_PATH, "-o", EXE_PATH],
                capture_output=True, text=True, timeout=60
            )
            if res.returncode != 0:
                st.error(f"Compilation error:\n{res.stderr}")
                return False
            if os.name != "nt":
                os.chmod(EXE_PATH, 0o755)
            return True
        except Exception as e:
            st.error(f"Compiler missing or build failure: {e}")
            return False
    return os.path.exists(EXE_PATH)

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
        st.error(f"Empty core output. Stderr:\n{res.stderr}")
        st.stop()

    try:
        return json.loads(res.stdout.strip())
    except json.JSONDecodeError:
        st.error(f"JSON Parsing Error:\n{res.stdout}")
        st.stop()

if "state" not in st.session_state:
    st.session_state.state = call_core("state")

THEME = {
    "H": {"bg": "linear-gradient(135deg, #064e3b 0%, #022c22 100%)", "border": "rgba(16, 185, 129, 0.2)", "icon": "🌱"},
    "W": {"bg": "linear-gradient(135deg, #78350f 0%, #451a03 100%)", "border": "rgba(245, 158, 11, 0.3)", "icon": "🌾"},
    "D": {"bg": "linear-gradient(135deg, #7f1d1d 0%, #450a0a 100%)", "border": "rgba(239, 68, 68, 0.3)", "icon": "🥀"}
}

# --- Sidebar Architecture ---
with st.sidebar:
    st.title("🚜 Navigation Metrics")
    st.markdown("""
    - **10x10 Matrix**: 100 Quadrants
    - **Routing Algorithm**: Greedy Nearest-Neighbor TSP
    - **Distance Metric**: Manhattan Distance
    - **Encapsulation**: Guarded internal coordinates
    - **Stack / Queue**: LIFO Rollbacks & FIFO Pipeline
    """)
    if st.button("🎲 Randomize 10x10 Field", use_container_width=True):
        st.session_state.state = call_core("reset")
        st.rerun()

st.title("🛰️ AgriBot 10x10 Autonomous Path Optimizer")

# Top Controller Panel
with st.expander("⚡ PATH OPTIMIZER & EXECUTION ENGINE", expanded=True):
    col_c1, col_c2, col_c3 = st.columns([1.2, 1, 1])
    with col_c1:
        anim_speed = st.slider("Step Traversal Speed (seconds)", min_value=0.05, max_value=0.8, value=0.15, step=0.05)
    with col_c2:
        if st.button("🧠 Compute Lowest-Cost Path", use_container_width=True):
            st.session_state.state = call_core("auto_optimize")
            st.rerun()
    with col_c3:
        if st.button("▶️ Execute Mission (Animated)", use_container_width=True):
            while st.session_state.state["queue"]:
                st.session_state.state = call_core("queue_next")
                render_matrix(matrix_placeholder, st.session_state.state)
                banner_placeholder.markdown(f"""
                <div class="status-banner">
                    <strong>TELEMETRY BUS:</strong> {st.session_state.state.get('msg', 'Navigating waypoint...')}
                </div>
                """, unsafe_allow_html=True)
                time.sleep(anim_speed)
            st.rerun()

banner_placeholder = st.empty()

col_map, col_ops = st.columns([1.6, 1], gap="large")

def render_matrix(target_container, current_state):
    with target_container.container():
        st.subheader("100-Quadrant Field Array (10x10)")
        for r in range(10):
            row_cols = st.columns(10)
            for c in range(10):
                ch = current_state["grid"][r][c]
                is_bot = (current_state["x"] == r and current_state["y"] == c)
                is_base = (r == 0 and c == 0)

                meta = THEME.get(ch, THEME["H"])
                cell_class = "cell-10"

                if is_bot:
                    cell_class += " robot-cell"
                    icon = "🤖"
                    border = "2px solid #4ade80"
                    bg = "linear-gradient(135deg, #14532d 0%, #052e16 100%)"
                elif is_base:
                    cell_class += " base-station"
                    icon = "⚡"
                    border = "2px dashed #38bdf8"
                    bg = "linear-gradient(135deg, #0c4a6e 0%, #082f49 100%)"
                else:
                    icon = meta["icon"]
                    border = f"1px solid {meta['border']}"
                    bg = meta["bg"]

                row_cols[c].markdown(
                    f"""
                    <div class="{cell_class}" style="background: {bg}; border: {border};">
                        <span>{icon}</span>
                        <span class="coord-tag">{r},{c}</span>
                    </div>
                    """,
                    unsafe_allow_html=True
                )

# --- Left Column: Grid ---
with col_map:
    matrix_placeholder = st.empty()
    render_matrix(matrix_placeholder, st.session_state.state)

    st.markdown("<br>", unsafe_allow_html=True)
    k1, k2, k3, k4 = st.columns(4)
    k1.markdown("🤖 **Rover**")
    k2.markdown("⚡ **Base Station [0,0]**")
    k3.markdown("🌾 **Weed Target**")
    k4.markdown("🥀 **Pathogen Target**")

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

    a1, a2, a3 = st.columns(3)
    if a1.button("💦 Precision Spray (-3%)", use_container_width=True):
        st.session_state.state = call_core("spray")
        st.rerun()
    if a2.button("⏪ Undo Stack", use_container_width=True):
        st.session_state.state = call_core("undo")
        st.rerun()
    if a3.button("⏩ Redo Stack", use_container_width=True):
        st.session_state.state = call_core("redo")
        st.rerun()

# --- Right Column: Diagnostics & Stacks ---
with col_ops:
    st.subheader("Field Diagnostics")

    batt = st.session_state.state["battery"]
    weeds_remaining = sum(row.count('W') for row in st.session_state.state["grid"])
    pathogens_remaining = sum(row.count('D') for row in st.session_state.state["grid"])

    d1, d2, d3 = st.columns(3)
    d1.metric("Battery Reserve", f"{batt}%")
    d2.metric("Grid Vector", f"[{st.session_state.state['x']}, {st.session_state.state['y']}]")
    d3.metric("Threats Left", f"{weeds_remaining + pathogens_remaining}")
    st.progress(batt / 100)

    st.markdown("---")
    st.subheader("Mission Pipeline (`std::queue`)")

    with st.expander("Pending Instructions", expanded=True):
        if st.session_state.state["queue"]:
            for idx, q_cmd in enumerate(st.session_state.state["queue"][:12], 1):
                clean_name = q_cmd.split(":")[0] + (" " + q_cmd.split(":")[1] if ":" in q_cmd else "")
                st.code(f"STEP #{idx}: {clean_name}")
            if len(st.session_state.state["queue"]) > 12:
                st.caption(f"...and {len(st.session_state.state['queue']) - 12} more instructions queued.")
        else:
            st.caption("No instructions in pipeline.")

    tab_u, tab_r, tab_h = st.tabs(["LIFO Undo", "LIFO Redo", "Flight History"])
    with tab_u:
        if st.session_state.state["undo"]:
            for item in reversed(st.session_state.state["undo"][-8:]):
                clean_name = item.split(":")[0] + (" " + item.split(":")[1] if ":" in item else "")
                st.text(f"⮌ {clean_name}")
        else:
            st.caption("Stack empty.")

    with tab_r:
        if st.session_state.state["redo"]:
            for item in reversed(st.session_state.state["redo"][-8:]):
                clean_name = item.split(":")[0] + (" " + item.split(":")[1] if ":" in item else "")
                st.text(f"⮎ {clean_name}")
        else:
            st.caption("Stack empty.")

    with tab_h:
        if st.session_state.state["history"]:
            for h in reversed(st.session_state.state["history"][-8:]):
                st.caption(f"• {h}")
        else:
            st.caption("No telemetry recorded.")

banner_placeholder.markdown(f"""
<div class="status-banner">
    <strong>TELEMETRY BUS:</strong> {st.session_state.state.get('msg', 'System active.')}
</div>
""", unsafe_allow_html=True)
