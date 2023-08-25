import { HttpClient, HttpEvent, HttpEventType } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { Observable, of } from 'rxjs';
import { ISystemInfo } from 'src/models/ISystemInfo';

import { environment } from '../../environments/environment';

@Injectable({
  providedIn: 'root'
})
export class SystemService {

  constructor(
    private httpClient: HttpClient
  ) { }

  public getInfo(): Observable<ISystemInfo> {
    if (environment.production) {
      return this.httpClient.get(`/api/system/info`) as Observable<ISystemInfo>;
    } else {
      return of(
        {
          "power": 11.670000076293945,
          "voltage": 5208.75,
          "current": 2237.5,
          "fanSpeed": 82,
          "hashRate": 0,
          "bestDiff": "0",
          "freeHeap": 200504,
          "coreVoltage": 1188,
          "ssid": "skimadtrees-secure",
          "wifiStatus": "Connected!",
          "sharesAccepted": 1,
          "sharesRejected": 0,
          "uptimeSeconds": 38,
          "ASICModel": "BM1366",
          "stratumURL": "192.168.1.242",
          "stratumPort": 3333,
          "stratumUser": "bc1q99n3pu025yyu0jlywpmwzalyhm36tg5u37w20d.bitaxe-U1"
        }
      );
    }
  }

  public restart() {
    return this.httpClient.post(`/api/system/restart`, {});
  }

  public updateSystem(update: any) {
    return this.httpClient.patch(`/api/system`, update);
  }

  public performOTAUpdate(file: File) {
    const reader = new FileReader();

    reader.onload = (event: any) => {
      const fileContent = event.target.result;

      return this.httpClient.post(`/api/system/OTA`, fileContent, {
        reportProgress: true,
        observe: 'events',
        responseType: 'text', // Specify the response type
        headers: {
          'Content-Type': 'application/octet-stream', // Set the content type
        },
      })
        .subscribe((event: HttpEvent<any>) => {
          if (event.type === HttpEventType.UploadProgress) {
            //this.progress = Math.round((event.loaded / event.total) * 100);
          } else if (event.type === HttpEventType.Response) {
            // Handle the response here
            console.log(event.body);
          }
        });
    };
    reader.readAsArrayBuffer(file);
  }


}
