%% Drag Rake Data Verification
% Plots and calculates drag (hopefully correctly) from tunnel pressure 
% data. Assumes static pressure is the same, all pressures referenced to 
% tunnel static ring at inlet

% test conditions: After wing AOA zeroed to ~ +-1 deg AOA of 0, AOA
% increased by 2 deg increments to 20 deg AOA
clc, clear

% constants
r = 273; % J/kg-k
L = 0.069; % m - rake length
AoA = 0:2:20; % deg - AoA vector

filenames = {'dr2_22_0d.mat', 'dr2_22_2d.mat','dr2_22_4d.mat', 'dr2_22_6d.mat', ...
    'dr2_22_8d.mat', 'dr2_22_10d.mat', 'dr2_22_12d.mat', 'dr2_22_14d.mat',...
    'dr2_22_16d.mat', 'dr2_22_18d.mat', 'dr2_22_20d.mat'};

graph_filenames = {'dr2_22_0d.png', 'dr2_22_2d.png','dr2_22_4d.png', 'dr2_22_6d.png', ...
    'dr2_22_8d.png', 'dr2_22_10d.png', 'dr2_22_12d.png', 'dr2_22_14d.png',...
    'dr2_22_16d.png', 'dr2_22_18d.png', 'dr2_22_20d.png'};


for jj = 1:length(filenames)
    load(filenames{jj})
        
    figure
    plot(t, P(:,1))
    hold on
    plot(t, P(:,3))
    plot(t, P(:,4))
    plot(t, P(:,5))
    plot(t, P(:,6))
    title(filenames{jj})
    xlabel('time (s)')
    ylabel('Pressure (pa)')
    legend('total', 'bot free', 'bot rake', 'top rake', 'top free')
    
    saveas(gcf,graph_filenames{jj})
    
    % finding density and average tunnel speed
    rho = r*T/P_amb; % kg/m^3
    speeds(jj) = mean(V);
    
    % calculating velocity at rake and freestream tubes - NOTE GAUGE
    % PRESSURE hence static pressure not needed
    v_inf_top(jj) = sqrt((2/rho)*(mean(P(:,6))));
    v_rake_top(jj) = sqrt((2/rho)*(mean(P(:,5))));
    v_rake_bot(jj) = sqrt((2/rho)*(mean(P(:,4))));
    v_inf_bot(jj) = sqrt((2/rho)*(mean(P(:,3))));
    v_tunnel(jj) = sqrt((2/rho)*(mean(P(:,1))));
    
    % calculating drag based on tunnel freestream vel and calc rake
    % freestream velocity
    drag_top_rake(jj) = rho*v_inf_top(jj)*(v_inf_top(jj) - v_rake_top(jj))*L/2;
    %drag_top_tunnel(ii,jj) = rho*mean(V)*(mean(V) - v_rake_top(ii,jj))*L/2;
    drag_bot_rake(jj) = rho*v_inf_bot(jj)*(v_inf_bot(jj) - v_rake_bot(jj))*L/2;
    %drag_bot_tunnel(ii,jj) = rho*mean(V)*(mean(V) - v_rake_bot(ii,jj))*L/2;
    drag_top_tunnel(jj) = rho*v_tunnel(jj)*(v_tunnel(jj) - v_rake_top(jj))*L/2;
    drag_bot_tunnel(jj) = rho*v_tunnel(jj)*(v_tunnel(jj) - v_rake_top(jj))*L/2;
    
    % summing total drag (rake and tunnel freestream velocity calcs)
    drag_rake(jj) = drag_top_rake(jj) + drag_bot_rake(jj);
    drag_raketunnel(jj) = drag_top_tunnel(jj) + drag_bot_tunnel(jj);
    
    % finding average pressure values for AOA trends
    P_av(1,jj) = mean(P(:,1));
    P_av(2,jj) = mean(P(:,3));
    P_av(3,jj) = mean(P(:,4));
    P_av(4,jj) = mean(P(:,5));
    P_av(5,jj) = mean(P(:,6));
    
    clear P P_amb T V
    
  
end

figure
plot(AoA, P_av)
title('Average pressures vs. AoA')
ylabel('Pressure (Pa)')
xlabel('AoA (deg)')
legend('Tunnel total', 'bot free', 'bot rake', 'top rake', 'top free')

saveas(gcf,'Av_Pressure_vs_AoA.png')

figure % comparing rake and tunnel drags
plot(AoA, drag_top_rake(1,:))
hold on
plot(AoA, drag_top_tunnel(1,:))
plot(AoA, drag_bot_rake(1,:))
plot(AoA, drag_bot_tunnel(1,:))
title('Drag vs. AoA')
ylabel('Drag (N)')
xlabel('AoA (deg)')
legend('Top - Rake freestream', 'Top - Tunnel freestream', ...
    'Bot - Rake freestream', 'Bot - Tunnel freestream')

figure % total drags
plot(AoA, drag_rake(1,:))
hold on
plot(AoA, drag_raketunnel(1,:))
title('Total drag vs. AoA')
ylabel('Drag (N)')
xlabel('AoA (deg)')
legend('Rake freestream', 'Tunnel freestream')