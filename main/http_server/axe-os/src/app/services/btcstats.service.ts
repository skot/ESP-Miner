import { Injectable } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { Observable } from 'rxjs';
import { IBtcInfo } from 'src/models/ISystemInfo'; // Make sure to import the interface

export interface NetworkStats {
  timestamp: number;
  market_price_usd: number;
  hash_rate: number;
  total_fees_btc: number;
  n_btc_mined: number;
  n_tx: number;
  n_blocks_mined: number;
  minutes_between_blocks: number;
  totalbc: number;
  n_blocks_total: number;
  estimated_transaction_volume_usd: number;
  blocks_size: number;
  miners_revenue_usd: number;
  nextretarget: number;
  difficulty: number;
  estimated_btc_sent: number;
  miners_revenue_btc: number;
  total_btc_sent: number;
  trade_volume_btc: number;
  trade_volume_usd: number;
}

@Injectable({
  providedIn: 'root'
})
export class BtcStatsService {
  private apiUrl = 'https://api.blockchain.info/stats';

  constructor(private http: HttpClient) {}

  getBitcoinStats(): Observable<IBtcInfo> {
    return this.http.get<IBtcInfo>(`${this.apiUrl}`); // Adjust the endpoint as necessary
  }
}
