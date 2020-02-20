%% Drag Rake Data Verification

clc, clear

% constants
r = 273; % J/kg-k


loadval = 'dr2_22_20d.mat';

load(loadval)

figure
plot(t, P)
title(loadval)
xlabel('time (s)')
ylabel('Pressure (pa)')
legend('total', 'static', 'bot free', 'bot rake', 'top rake', 'top free')
ylim([P(1,6)-3 P(1,1)+3])

% % finding density and average tunnel speed
% rho = r*T/P_amb; % kg/m^3
% speed = mean(V);
% 
% % calculating velocity at rake and freestream tubes
% v_inf_top = sqrt((2/rho)*(mean(P(:,1)-P(:,6))));
% v_rake_top = sqrt((2/rho)*(mean(P(:,1)-P(:,5))));
% v_rake_bot = sqrt((2/rho)*(mean(P(:,1)-P(:,4))));
% v_inf_bot = sqrt((2/rho)*(mean(P(:,1)-P(:,3))));
% 
% 
% % calculating drag based on tunnel freestream vel and calc rake
% % freestream velocity
% drag_top_rake = rho*v_inf_top*(v_inf_top - v_rake_top);
% drag_top_tunnel = rho*mean(V)*(mean(V) - v_rake_top);
% drag_bot_rake = rho*v_inf_bot*(v_inf_bot - v_rake_bot);
% drag_bot_tunnel = rho*mean(V)*(mean(V) - v_rake_bot);
% 
% % summing total drag (rake and tunnel freestream velocity calcs)
% drag_rake = drag_top_rake + drag_bot_rake;
% drag_raketunnel = drag_top_tunnel + drag_bot_tunnel;
% 
% %disp("
% 
% figure % comparing rake and tunnel drags
% plot(speeds, drag_top_rake(:,1))
% hold on
% plot(speeds, drag_top_tunnel(:,1))
% plot(speeds, drag_bot_rake(:,1))
% plot(speeds, drag_bot_tunnel(:,1))
% title('Drag vs. Speed')
% ylabel('Drag (N)')
% xlabel('Speed (m/s)')
% legend('Top - Rake freestream', 'Top - Tunnel freestream', ...
%     'Bot - Rake freestream', 'Bot - Tunnel freestream')
% 
% figure % total drags
% plot(speeds, drag_rake(:,1))
% hold on
% plot(speeds, drag_raketunnel(:,1))
% title('Total drag vs. Speed')
% ylabel('Drag (N)')
% xlabel('Speed (m/s)')
% legend('Rake freestream', 'Tunnel freestream')
% 
