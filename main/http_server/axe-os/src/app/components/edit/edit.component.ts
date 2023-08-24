import { Component } from '@angular/core';
import { FormBuilder, FormGroup, Validators } from '@angular/forms';
import { SystemService } from 'src/app/services/system.service';

@Component({
  selector: 'app-edit',
  templateUrl: './edit.component.html',
  styleUrls: ['./edit.component.scss']
})
export class EditComponent {

  public form!: FormGroup;

  constructor(
    private fb: FormBuilder,
    private systemService: SystemService
  ) {
    this.systemService.getInfo().subscribe(info => {
      this.form = this.fb.group({
        stratumURL: [info.stratumURL, [Validators.required]],
        stratumPort: [info.stratumPort, [Validators.required]],
        stratumUser: [info.stratumUser, [Validators.required]]
      });
    });
  }
}
