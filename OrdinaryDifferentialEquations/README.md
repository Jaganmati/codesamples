This is a C++ program that plots graph data for different ODEs. It uses an ImGui interface to allow for user input of the generated graph. A separate Python program is used to display the graph. This program can generate First Order ODEs, Second Order ODEs, and Systems of ODEs, all using either the Euler, Midpoint, Modified Euler, or Runge-Kutta methods to do so.<br><br>
An example of the output of the First Order ODE using Runge-Kutta:<br>
![FirstODE](images/FirstODE.png)<br>
f(t, y) = y - t + 2^(-y)<br><br>
An example of the output of the Second Order ODE using Runge-Kutta:<br>
![SecondODE](images/SecondODE.png)<br>
f_1(t, x, v) = v<br>
f_2(t, y, v) = t - y - v<br><br>
An example of the output of a System of ODEs using Runge-Kutta:<br>
![SystemODE](images/SystemODE.png)<br>
f_1(t, x, y) = x - 0.1 * x * y<br>
f_2(t, x, y) = -0.5 * y + 0.01 * x * y
