import { Component } from '@angular/core';
import { Observable } from 'rxjs';
import { SystemService } from 'src/app/services/system.service';

@Component({
  selector: 'app-home',
  templateUrl: './home.component.html',
  styleUrls: ['./home.component.scss']
})
export class HomeComponent {

  public info$: Observable<any>;

  constructor(private systemService: SystemService) {
    this.info$ = this.systemService.getInfo();
  }

  public restart() {
    this.systemService.restart().subscribe(res => {

    })
  }
}
