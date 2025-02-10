import matplotlib.pyplot as plt

# Load the data
file_path = r"C:\Users\Craig\Documents\GitHub\scv\src\visualizer\output.txt"

with open(file_path, "r") as file:
    lines = file.readlines()

# Parse the data
time, x, y, z, e, s = [], [], [], [], [], []

for line in lines[1:]:  # Skip the header
    values = line.split()
    if len(values) == 6:
        s, t, x_val, y_val, z_val, e_val = map(float, values)
        time.append(t)
        x.append(x_val)
        y.append(y_val)
        z.append(z_val)
        e.append(e_val )

# Plot the data
plt.figure(figsize=(10, 6))

plt.plot(time, x, label="X", linestyle="-")
plt.plot(time, y, label="Y", linestyle="--")
plt.plot(time, z, label="Z", linestyle=":")
plt.plot(time, e, label="E", linestyle="-.")

plt.xlabel("Time")
plt.ylabel("Values")
plt.title("Data Plot")
plt.legend()
plt.grid()

plt.show()
