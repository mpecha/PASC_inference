function weak_repeat_and_save(filename_begin,C_small,noise_small,gamma0_small,nmb_of_repeat,Ns)

for Ni = 1:length(Ns)
    N = Ns(Ni);
    
    % First we create the artificial time series signal C_true
    C_true=repmat(C_small,1,nmb_of_repeat*N);
    disp([' - size of C_true: ' num2str(length(C_true))])

    % save solution
    filename = strcat(filename_begin, ['_' num2str(N) '_solution.bin']);
    savebin( filename, C_true );

    % signal with noise
    C=repmat(C_small+noise_small,1,nmb_of_repeat*N);
    filename = strcat(filename_begin, ['_' num2str(N) '_data.bin']);
    savebin( filename, C );

    % generate gamma0
    K = length(gamma0_small)/length(C_small);
    gamma0 = zeros(1,length(C)*K);
    for k = 1:K
        gamma0(1,((k-1)*length(C)+1) : (k*length(C))) = repmat(gamma0_small(1:length(C_small)),1,nmb_of_repeat*N); 
    end
    filename = strcat(filename_begin, ['_' num2str(N) '_gamma0.bin']);
    savebin( filename, gamma0 );

    if false
        figure
        hold on
        plot(1:length(C),C,'b');
        plot(1:length(C),C_true,'r');
        hold off
    end
    
end

end