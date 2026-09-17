import type { SystemStatus, Sequence } from './interfaces';

export type ApiResponse<T> = {
    data: T | null;
    error: string | null;
    loading: boolean;
};

export async function apiRequest<T>(
    url: string, 
    method: 'GET' | 'PUT' | 'POST' = 'GET', 
    body?: any
): Promise<ApiResponse<T>> {
    try {
        const options: RequestInit = {
            method,
            headers: { 'Content-Type': 'application/json' },
        };

        if (body) options.body = JSON.stringify(body);

        const response = await fetch(url, options);

        if (!response.ok) {
            return { data: null, error: `Server Error: ${response.status}`, loading: false };
        }

        const data = await response.json();
        return { data, error: null, loading: false };
    } catch (err) {
        return { 
            data: null, 
            error: err instanceof Error ? err.message : "Network unreachable", 
            loading: false 
        };
    }
}