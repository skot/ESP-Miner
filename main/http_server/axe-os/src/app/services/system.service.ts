import { HttpClient } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { of } from 'rxjs';

import { environment } from '../../environments/environment';

@Injectable({
  providedIn: 'root'
})
export class SystemService {

  constructor(
    private httpClient: HttpClient
  ) { }

  public getInfo() {
    if (environment.production) {
      return this.httpClient.get(`/api/system/info`);
    } else {
      return of(
        {
          "power": 11.640000343322754,
          "voltage": 5207.5,
          "current": 2235,
          "fanSpeed": 82,
          "hashRate": 250.09659830025382,
          "bestDiff": "6.13k",
          "freeHeap": 199604,
          "coreVoltage": 1185,
          "ssid": "skimadtrees-secure",
          "wifiStatus": "Connected!",
          "sharesAccepted": 62,
          "sharesRejected": 62,
          "uptimeSeconds": 274
        }
      );
    }
  }
}
