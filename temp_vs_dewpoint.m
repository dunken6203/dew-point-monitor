% ThingSpeak MATLAB Visualization
% Cabinet temperature vs dew point, with warning line and warning markers.
% The closer the red line gets to the blue line, the higher the condensation risk.

readChannelID = 1234567;              % your channel ID
readAPIKey    = 'YOUR_READ_API_KEY';  % Channel > API Keys > Read API Key
margin        = 3;                    % same as DEW_MARGIN in the firmware

% Field 1 = A temp, Field 3 = B temp, Field 5 = A dew point
[data, time] = thingSpeakRead(readChannelID, ...
    'Fields', [1 3 5], 'NumMinutes', 60, 'ReadKey', readAPIKey);
time = time + hours(8);               % UTC -> Taiwan time

tA    = data(:,1);
tB    = data(:,2);
dpA   = data(:,3);
alarm = (tA - dpA) < margin;          % judged here, so changing margin re-evaluates history

plot(time, tA,  'r-', 'LineWidth', 1.8); hold on
plot(time, tB,  'Color', [0.5 0.5 0.5], 'LineWidth', 1.2);
plot(time, dpA, 'b-', 'LineWidth', 1.8);
plot(time, dpA + margin, 'b:', 'LineWidth', 1.2);
plot(time(alarm), tA(alarm), 'o', 'MarkerSize', 6, ...
     'MarkerEdgeColor', 'r', 'MarkerFaceColor', 'y');
hold off

grid on
xlabel('Time');
ylabel('Temperature (\circC)');

if ~isempty(alarm) && alarm(end)
    title('DEW WARNING!', 'Color', 'r');
else
    title(sprintf('Cabinet Temp vs Dew Point (warnings: %d)', sum(alarm)));
end

lgd = legend('A temp (inside)', 'B temp (ambient)', 'A dew point', ...
             'Warning (dew pt + margin)', 'Dew warning', ...
             'Location', 'southoutside', 'NumColumns', 2);
lgd.FontSize = 8;
