#!/bin/sh
start_x21led() {
    echo "Starting x21b-led-helper"
    x21b-led-helper
}

stop_x21led() {
    :
}

restart_x21led() {
    :
}

status_x21led() {
    :
}

case "$1" in
    start)
        start_x21led
        ;;
    stop)
        stop_x21led
        ;;
    restart)
        restart_x21led
        ;;
    status)
        status_x21led
        ;;
    *)
        echo "Usage: $0 {start|stop|restart|status}"
        exit 1
        ;;
esac

exit 0
