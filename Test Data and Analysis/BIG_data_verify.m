%% Drag Rake Data Verification

clc, clear

% constants
r = 273; % J/kg-k
L = 0.069; % m - rake length

filenames = {'dr_11.5_0c.mat', 'dr_16_0c.mat', 'dr_22_0c.mat',...
    'dr_27_0c.mat', 'dr_32_0c.mat', 'dr_37_0c.mat', 'dr_42_0c.mat';... 
    'dr_11.5_0d.mat', 'dr_16_0d.mat', 'dr_22_0d.mat', 'dr_27_0d.mat',...
    'dr_32_0d.mat', 'dr_37_0d.mat', 'dr_42_0d.mat'};

graph_filenames = {'dr_11.5_0c.png', 'dr_16_0c.png', 'dr_22_0c.png',...
    'dr_27_0c.png', 'dr_32_0c.png', 'dr_37_0c.png', 'dr_42_0c.png';... 
    'dr_11.5_0d.png', 'dr_16_0d.png', 'dr_22_0d.png', 'dr_27_0d.png',...
    'dr_32_0d.png', 'dr_37_0d.png', 'dr_42_0d.png'};


for jj = 1:length(filenames)
    for ii = 1:2
    load(filenames{ii,jj})
        
    figure
    plot(t, P(:,1))
    hold on
    plot(t, P(:,3))
    plot(t, P(:,4))
    plot(t, P(:,5))
    plot(t, P(:,6))
    title(filenames{ii,jj})
    xlabel('time (s)')
    ylabel('Pressure (pa)')
    legend('total', 'bot free', 'bot rake', 'top rake', 'top free')
    
    % saveas(gcf,graph_filenames{ii,jj})
    
    % finding density and average tunnel speed
    rho = r*T/P_amb; % kg/m^3
    speeds(jj) = mean(V);
    
    % calculating velocity at rake and freestream tubes - NOTE GAUGE
    % PRESSURE hence static pressure not needed
    v_inf_top(ii,jj) = sqrt((2/rho)*(mean(P(:,6))));
    v_rake_top(ii,jj) = sqrt((2/rho)*(mean(P(:,5))));
    v_rake_bot(ii,jj) = sqrt((2/rho)*(mean(P(:,4))));
    v_inf_bot(ii,jj) = sqrt((2/rho)*(mean(P(:,3))));
    v_tunnel(ii,jj) = sqrt((2/rho)*(mean(P(:,1))));
    
    % calculating drag based on tunnel freestream vel and calc rake
    % freestream velocity
    drag_top_rake(ii,jj) = rho*v_inf_top(ii,jj)*(v_inf_top(ii,jj) - v_rake_top(ii,jj))*L/2;
    %drag_top_tunnel(ii,jj) = rho*mean(V)*(mean(V) - v_rake_top(ii,jj))*L/2;
    drag_bot_rake(ii,jj) = rho*v_inf_bot(ii,jj)*(v_inf_bot(ii,jj) - v_rake_bot(ii,jj))*L/2;
    %drag_bot_tunnel(ii,jj) = rho*mean(V)*(mean(V) - v_rake_bot(ii,jj))*L/2;
    drag_top_tunnel(ii,jj) = rho*v_tunnel(ii,jj)*(v_tunnel(ii,jj) - v_rake_top(ii,jj))*L/2;
    drag_bot_tunnel(ii,jj) = rho*v_tunnel(ii,jj)*(v_tunnel(ii,jj) - v_rake_top(ii,jj))*L/2;
    
    % summing total drag (rake and tunnel freestream velocity calcs)
    drag_rake(ii,jj) = drag_top_rake(ii,jj) + drag_bot_rake(ii,jj);
    drag_raketunnel(ii,jj) = drag_top_tunnel(ii,jj) + drag_bot_tunnel(ii,jj);
    
    clear P P_amb T V
    
    end
end

figure % calculated velocities vs tunnel speed
plot(speeds, v_tunnel(1,:))
hold on
plot(speeds, v_rake_top(1,:))
plot(speeds, v_inf_top(1,:))
plot(speeds, v_rake_bot(1,:))
plot(speeds, v_inf_bot(1,:))
plot(speeds, v_tunnel(2,:))
plot(speeds, v_rake_top(2,:))
plot(speeds, v_inf_top(2,:))
plot(speeds, v_rake_bot(2,:))
plot(speeds, v_inf_bot(2,:))
title('Rake speeds vs. Tunnel speed (Not very helpful lol)')
ylabel('Rake speed (m/s)')
xlabel('Speed (m/s)')
legend('Tunnel', 'Top - Rake freestream', 'Top - Tunnel freestream', ...
    'Bot - Rake freestream', 'Bot - Tunnel freestream', 'Tunnel (flipped)', ...
    'Top - Rake freestream (flipped)', 'Top - Tunnel freestream (flipped)', ...
    'Bot - Rake freestream (flipped)', 'Bot - Tunnel freestream (flipped)')

figure % comparing rake and tunnel drags
plot(speeds, drag_top_rake(1,:))
hold on
plot(speeds, drag_top_tunnel(1,:))
plot(speeds, drag_bot_rake(1,:))
plot(speeds, drag_bot_tunnel(1,:))
plot(speeds, drag_top_rake(2,:))
plot(speeds, drag_top_tunnel(2,:))
plot(speeds, drag_bot_rake(2,:))
plot(speeds, drag_bot_tunnel(2,:))
title('Drag vs. Speed')
ylabel('Drag (N)')
xlabel('Speed (m/s)')
legend('Top - Rake freestream', 'Top - Tunnel freestream', ...
    'Bot - Rake freestream', 'Bot - Tunnel freestream',...
    'Top - Rake freestream (flipped)', 'Top - Tunnel freestream (flipped)', ...
    'Bot - Rake freestream (flipped)', 'Bot - Tunnel freestream (flipped)')

figure % total drags
plot(speeds, drag_rake(1,:))
hold on
plot(speeds, drag_raketunnel(1,:))
plot(speeds, drag_rake(2,:))
plot(speeds, drag_raketunnel(2,:))
title('Total drag vs. Speed')
ylabel('Drag (N)')
xlabel('Speed (m/s)')
legend('Rake freestream', 'Tunnel freestream', 'Rake freestream (flipped)'...
    , 'Tunnel freestream (flipped)')