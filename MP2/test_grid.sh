./manager_send 0 send 63 "message from 0 to 63";
./manager_send 63 send 0 "message from 63 to 0";
./manager_send 7 send 57 "message from 7 to 57";
./manager_send 57 send 7 "message from 57 to 7";

sudo iptables -D OUTPUT -s 10.1.1.62 -d 10.1.1.63 -j ACCEPT;
sudo iptables -D OUTPUT -s 10.1.1.63 -d 10.1.1.62 -j ACCEPT;
sudo iptables -D OUTPUT -s 10.1.1.55 -d 10.1.1.63 -j ACCEPT;
sudo iptables -D OUTPUT -s 10.1.1.63 -d 10.1.1.55 -j ACCEPT;
sleep 2.0;
./manager_send 0 send 63 "message from 0 to 63";
./manager_send 63 send 0 "message from 63 to 0";
sleep 5.0e-1;

sudo iptables -I OUTPUT -s 10.1.1.62 -d 10.1.1.63 -j ACCEPT;
sudo iptables -I OUTPUT -s 10.1.1.63 -d 10.1.1.62 -j ACCEPT;
sudo iptables -I OUTPUT -s 10.1.1.55 -d 10.1.1.63 -j ACCEPT;
sudo iptables -I OUTPUT -s 10.1.1.63 -d 10.1.1.55 -j ACCEPT;
sleep 5.0e-1;
./manager_send 0 send 63 "message from 0 to 63";
./manager_send 63 send 0 "message from 63 to 0";
sleep 5.0e-1;

./manager_send 1 cost 2 10;
./manager_send 7 cost 6 10;
sleep 5.0e-1;
./manager_send 0 send 63 "message from 0 to 63";
./manager_send 7 send 57 "message from 7 to 57";
sleep 5.0e-1;

./manager_send 1 cost 2 1;
./manager_send 7 cost 6 1;
sleep 5.0e-1;
./manager_send 0 send 63 "message from 0 to 63";
./manager_send 7 send 57 "message from 7 to 57";
