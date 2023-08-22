import { Component } from '@angular/core';
import { Observable } from 'rxjs';

import { SystemService } from './services/system.service';

@Component({
  selector: 'app-root',
  templateUrl: './app.component.html',
  styleUrls: ['./app.component.scss']
})
export class AppComponent {

  public info$: Observable<any>;
  constructor(private systemService: SystemService) {
    this.info$ = this.systemService.getInfo();
  }
}
