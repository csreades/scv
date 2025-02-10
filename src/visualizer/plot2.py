import numpy as np
import matplotlib.pyplot as plt

# Load data manually
file_path = r"C:\Users\Craig\Documents\GitHub\scv\src\visualizer\output.txt"
time, x, y, z = [], [], [], []

skipFirst = True

with open(file_path, "r") as f:
    for line in f:
        if skipFirst:
            skipFirst = False
        else:
            t, x_pos, y_pos, z_pos, e_pos = map(float, line.split())
            time.append(t)
            x.append(x_pos)
            y.append(y_pos)
            z.append(z_pos)

# Convert lists to NumPy arrays for calculations
time = np.array(time)
x = np.array(x)
y = np.array(y)
z = np.array(z)

# Compute velocity components
vx = np.gradient(x, time)
vy = np.gradient(y, time)
vz = np.gradient(z, time)

# Compute total speed
speed = np.sqrt(vx**2 + vy**2 + vz**2)

# Compute acceleration components
ax = np.gradient(vx, time)
ay = np.gradient(vy, time)
az = np.gradient(vz, time)

# Compute total acceleration
acceleration = np.sqrt(ax**2 + ay**2 + az**2)

# Compute jerk components (rate of change of acceleration)
jx = np.gradient(ax, time)
jy = np.gradient(ay, time)
jz = np.gradient(az, time)

# Compute total jerk
jerk = np.sqrt(jx**2 + jy**2 + jz**2)

# Create subplots
fig, axs = plt.subplots(4, 1, figsize=(10, 12), sharex=True)

# Plot XYZ position
axs[0].plot(time, x, label="X")
axs[0].plot(time, y, label="Y")
axs[0].plot(time, z, label="Z")
axs[0].set_ylabel("Position")
axs[0].legend()
axs[0].set_title("XYZ Position Over Time")

# Plot speed
axs[1].plot(time, speed, label="Speed", color="blue")
axs[1].set_ylabel("Speed")
axs[1].legend()
axs[1].set_title("Total Speed Over Time")

# Plot acceleration
axs[2].plot(time, acceleration, label="Acceleration", color="red")
axs[2].set_ylabel("Acceleration")
axs[2].legend()
axs[2].set_title("Total Acceleration Over Time")

# Plot jerk
axs[3].plot(time, jerk, label="Jerk", color="purple")
axs[3].set_xlabel("Time")
axs[3].set_ylabel("Jerk")
axs[3].legend()
axs[3].set_title("Total Jerk Over Time")

plt.tight_layout()
plt.show()
