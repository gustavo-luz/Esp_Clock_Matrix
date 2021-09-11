while true; do
  date $'+\x80%T' >/dev/ttyUSB0
  sleep 0.1
done
