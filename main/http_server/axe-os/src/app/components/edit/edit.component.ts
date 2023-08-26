import { HttpErrorResponse, HttpEventType } from '@angular/common/http';
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

  public firmwareUpdateProgress: number | null = null;
  public websiteUpdateProgress: number | null = null;

  constructor(
    private fb: FormBuilder,
    private systemService: SystemService,
    private toastr: ToastrService,
    private toastrService: ToastrService
  ) {
    this.systemService.getInfo().subscribe(info => {
      this.form = this.fb.group({
        stratumURL: [info.stratumURL, [Validators.required]],
        stratumPort: [info.stratumPort, [Validators.required]],
        stratumUser: [info.stratumUser, [Validators.required]],
        ssid: [info.ssid, [Validators.required]],
        wifiPass: [info.wifiPass, [Validators.required]],
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

  otaUpdate(event: any) {
    const file = event.target?.files.item(0) as File;

    if (file.name != 'esp-miner.bin') {
      this.toastrService.error('Incorrect file, looking for esp-miner.bin.', 'Error');
      return;
    }

    this.systemService.performOTAUpdate(file).subscribe({
      next: (event) => {
        if (event.type === HttpEventType.UploadProgress) {
          this.firmwareUpdateProgress = Math.round((event.loaded / (event.total as number)) * 100);
        } else if (event.type === HttpEventType.Response) {
          if (event.ok) {
            this.toastrService.success('Firmware updated', 'Success!');

          } else {
            this.toastrService.error(event.statusText, 'Error');
          }
        }
      },
      error: (err) => {
        this.toastrService.error(event.statusText, 'Error');
      },
      complete: () => {
        this.firmwareUpdateProgress = null;
      }
    });
  }
  otaWWWUpdate(event: any) {
    const file = event.target?.files.item(0) as File;
    if (file.name != 'www.bin') {
      this.toastrService.error('Incorrect file, looking for www.bin.', 'Error');
      return;
    }

    this.systemService.performWWWOTAUpdate(file).subscribe({
      next: (event) => {
        if (event.type === HttpEventType.UploadProgress) {
          this.websiteUpdateProgress = Math.round((event.loaded / (event.total as number)) * 100);
        } else if (event.type === HttpEventType.Response) {
          if (event.ok) {
            this.toastrService.success('Website updated', 'Success!');
            window.location.reload();
          } else {
            this.toastrService.error(event.statusText, 'Error');
          }
        }
      },
      error: (err) => {
        this.toastrService.error(event.statusText, 'Error');
      },
      complete: () => {
        this.websiteUpdateProgress = null;
      }
    });

  }
}
