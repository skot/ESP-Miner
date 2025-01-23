import { HttpErrorResponse, HttpEventType } from '@angular/common/http';
import { Component, ViewChild } from '@angular/core';
import { FormBuilder, FormGroup, Validators } from '@angular/forms';
import { ToastrService } from 'ngx-toastr';
import { FileUploadHandlerEvent, FileUpload } from 'primeng/fileupload';
import { map, Observable, shareReplay, startWith } from 'rxjs';
import { GithubUpdateService } from 'src/app/services/github-update.service';
import { LoadingService } from 'src/app/services/loading.service';
import { SystemService } from 'src/app/services/system.service';
import { eASICModel } from 'src/models/enum/eASICModel';

@Component({
  selector: 'app-settings',
  templateUrl: './settings.component.html',
  styleUrls: ['./settings.component.scss']
})
export class SettingsComponent {

  public form!: FormGroup;

  public firmwareUpdateProgress: number | null = null;
  public websiteUpdateProgress: number | null = null;


  public devToolsOpen: boolean = false;
  public eASICModel = eASICModel;
  public ASICModel!: eASICModel;

  public checkLatestRelease: boolean = false;
  public latestRelease$: Observable<any>;

  public info$: Observable<any>;

  @ViewChild('firmwareUpload') firmwareUpload!: FileUpload;
  @ViewChild('websiteUpload') websiteUpload!: FileUpload;

  constructor(
    private fb: FormBuilder,
    private systemService: SystemService,
    private toastr: ToastrService,
    private toastrService: ToastrService,
    private loadingService: LoadingService,
    private githubUpdateService: GithubUpdateService
  ) {



    window.addEventListener('resize', this.checkDevTools);
    this.checkDevTools();

    this.latestRelease$ = this.githubUpdateService.getReleases().pipe(map(releases => {
      return releases[0];
    }));

    this.info$ = this.systemService.getInfo().pipe(shareReplay({refCount: true, bufferSize: 1}))


      this.info$.pipe(this.loadingService.lockUIUntilComplete())
      .subscribe(info => {
        this.ASICModel = info.asicModel;
        this.form = this.fb.group({
          flipScreen: [info.flipScreen == 1],
          invertScreen: [info.invertScreen == 1],
          stratumURL: [info.stratumURL, [
            Validators.required,
            Validators.pattern(/^(?!.*stratum\+tcp:\/\/).*$/),
            Validators.pattern(/^[^:]*$/),
          ]],
          stratumPort: [info.stratumPort, [
            Validators.required,
            Validators.pattern(/^[^:]*$/),
            Validators.min(0),
            Validators.max(65353)
          ]],
          stratumUser: [info.stratumUser, [Validators.required]],
          stratumPassword: ['*****', [Validators.required]],
          coreVoltage: [info.coreVoltage, [Validators.required]],
          frequency: [info.frequency, [Validators.required]],
          autoFanSpeed: [info.autoFanSpeed == 1, [Validators.required]],
          invertFanPolarity: [info.invertFanPolarity == 1, [Validators.required]],
          fanSpeed: [info.fanSpeed, [Validators.required]],
        });

        this.form.controls['autoFanSpeed'].valueChanges.pipe(
          startWith(this.form.controls['autoFanSpeed'].value)
        ).subscribe(autofanspeed => {
          if (autofanspeed) {
            this.form.controls['fanSpeed'].disable();
          } else {
            this.form.controls['fanSpeed'].enable();
          }
        });
      });

  }
  private checkDevTools = () => {
    if (
      window.outerWidth - window.innerWidth > 160 ||
      window.outerHeight - window.innerHeight > 160
    ) {
      this.devToolsOpen = true;
    } else {
      this.devToolsOpen = false;
    }
  };

  public updateSystem() {

    const form = this.form.getRawValue();

    form.frequency = parseInt(form.frequency);
    form.coreVoltage = parseInt(form.coreVoltage);

    // bools to ints
    form.flipScreen = form.flipScreen == true ? 1 : 0;
    form.invertScreen = form.invertScreen == true ? 1 : 0;
    form.invertFanPolarity = form.invertFanPolarity == true ? 1 : 0;
    form.autoFanSpeed = form.autoFanSpeed == true ? 1 : 0;

    if (form.stratumPassword === '*****') {
      delete form.stratumPassword;
    }

    this.systemService.updateSystem(undefined, form)
      .pipe(this.loadingService.lockUIUntilComplete())
      .subscribe({
        next: () => {
          this.toastr.success('Success!', 'Saved.');
        },
        error: (err: HttpErrorResponse) => {
          this.toastr.error('Error.', `Could not save. ${err.message}`);
        }
      });
  }

  otaUpdate(event: FileUploadHandlerEvent) {
    const file = event.files[0];
    this.firmwareUpload.clear(); // clear the file upload component

    if (file.name != 'esp-miner.bin') {
      this.toastrService.error('Incorrect file, looking for esp-miner.bin.', 'Error');
      return;
    }

    this.systemService.performOTAUpdate(file)
      .pipe(this.loadingService.lockUIUntilComplete())
      .subscribe({
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
          else if (event instanceof HttpErrorResponse)
          {
            this.toastrService.error(event.error, 'Error');
          }
        },
        error: (err) => {
          this.toastrService.error(err.error, 'Error');
        },
        complete: () => {
          this.firmwareUpdateProgress = null;
        }
      });
  }

  otaWWWUpdate(event: FileUploadHandlerEvent) {
    const file = event.files[0];
    this.websiteUpload.clear(); // clear the file upload component

    if (file.name != 'www.bin') {
      this.toastrService.error('Incorrect file, looking for www.bin.', 'Error');
      return;
    }

    this.systemService.performWWWOTAUpdate(file)
      .pipe(
        this.loadingService.lockUIUntilComplete(),
      ).subscribe({
        next: (event) => {
          if (event.type === HttpEventType.UploadProgress) {
            this.websiteUpdateProgress = Math.round((event.loaded / (event.total as number)) * 100);
          } else if (event.type === HttpEventType.Response) {
            if (event.ok) {
              this.toastrService.success('Website updated', 'Success!');
              setTimeout(() => {
                window.location.reload();
              }, 2000);
            } else {
              this.toastrService.error(event.statusText, 'Error');
            }
          }
          else if (event instanceof HttpErrorResponse)
          {
            const errorMessage = event.error?.message || event.message || 'Unknown error occurred';
            this.toastrService.error(errorMessage, 'Error');
          }
        },
        error: (err) => {
          const errorMessage = err.error?.message || err.message || 'Unknown error occurred';
          this.toastrService.error(errorMessage, 'Error');
        },
        complete: () => {
          this.websiteUpdateProgress = null;
        }
      });
  }

  public restart() {
    this.systemService.restart().subscribe(res => {

    });
    this.toastr.success('Success!', 'Bitaxe restarted');
  }
}
