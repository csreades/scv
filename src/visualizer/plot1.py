import matplotlib.pyplot as plt

# Load the data
file_path = r"C:\Users\Craig\Documents\GitHub\scv\src\visualizer\output.txt"

with open(file_path, "r") as file:
    lines = file.readlines()

# Parse the data
time, x, y, z, e, s, v, a = [], [], [], [], [], [], [], []

for line in lines[1:]:  # Skip the header
    values = line.split()
    if len(values) == 8:
        s_val, t, x_val, y_val, z_val, e_val, _, _ = map(float, values)
        s.append(s_val)
        time.append(t)
        x.append(x_val)
        y.append(y_val)
        z.append(z_val)
        e.append(e_val)
        
        if len(x) > 2:
            v.append((x[-1] - x[-2]) / 0.002)
        else:
            v.append(0)
            
        if len(x) > 3:
            a.append((v[-1] - v[-2]) / 0.002)
        else:
            a.append(0)

# Create the figure and first axis
fig, ax1 = plt.subplots(figsize=(10, 6))

# Plot X on the first axis
ax1.set_xlabel("Time (s)")
ax1.set_ylabel("X Position", color="tab:blue")
ax1.plot(time, x, label="X", color="tab:blue", linestyle="-")
ax1.tick_params(axis="y", labelcolor="tab:blue")

# Create the second axis for velocity
ax2 = ax1.twinx()
ax2.set_ylabel("Velocity", color="tab:orange")
ax2.plot(time, v, label="V", color="tab:orange", linestyle="--")
ax2.tick_params(axis="y", labelcolor="tab:orange")

# Create the third axis for acceleration
ax3 = ax1.twinx()
ax3.spines["right"].set_position(("outward", 60))
ax3.set_ylabel("Acceleration", color="tab:green")
ax3.plot(time, a, label="A", color="tab:green", linestyle=":")
ax3.tick_params(axis="y", labelcolor="tab:green")

# Create the fourth axis for E
ax4 = ax1.twinx()
ax4.spines["right"].set_position(("outward", 120))
ax4.set_ylabel("E", color="tab:red")
ax4.plot(time, e, label="E", color="tab:red", linestyle="-.")
ax4.tick_params(axis="y", labelcolor="tab:red")

# Title and layout
fig.suptitle("Multi-Axis Data Plot")
fig.tight_layout()

plt.show()
