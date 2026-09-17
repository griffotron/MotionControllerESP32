type Listener = (data: any) => void;

export class SocketService {
    public lastMessage = $state<any>(null);
    public isConnected = $state(false);
    
    private socket: WebSocket | null = null;
    private listeners = new Set<Listener>();

    subscribe(callback: Listener){
        this.listeners.add(callback);
        return () => this.listeners.delete(callback);
    }

    connect(url: string) {
        if (this.socket) return;

        this.socket = new WebSocket(url);
        this.socket.onopen = () => this.isConnected = true;
        this.socket.onclose = () => this.isConnected = false;
        
        this.socket.onmessage = (event) => {
            let obj = JSON.parse(event.data);
            this.lastMessage = obj
            this.listeners.forEach(fn => fn(obj));
        };
    }

    send(data: object) {
        if (this.socket?.readyState === WebSocket.OPEN) {
            this.socket.send(JSON.stringify(data));
        }
        else{
            console.log("Socket closed. Retrying in 500ms");
            //TODO: maybe try opening the socket here
            setTimeout(() => {this.send(data);}, 500);
        }
    }
}
//single instance for use in other components
export const socketService = new SocketService();