import { HttpErrorResponse } from '@angular/common/http';
import { Component } from '@angular/core';
import { FormBuilder, FormGroup, Validators } from '@angular/forms';
import { ToastrService } from 'ngx-toastr';
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
    private systemService: SystemService,
    private toastr: ToastrService
  ) {
    this.systemService.getInfo().subscribe(info => {
      this.form = this.fb.group({
        stratumURL: [info.stratumURL, [Validators.required]],
        stratumPort: [info.stratumPort, [Validators.required]],
        stratumUser: [info.stratumUser, [Validators.required]]
      });
    });
  }

  public updateSystem() {
    this.systemService.updateSystem(this.form.value).subscribe({
      next: () => {
        this.toastr.success('Success!', 'Saved.');
      },
      error: (err: HttpErrorResponse) => {
        this.toastr.error('Error.', `Could not save. ${err.message}`);
      }
    });
  }
}
