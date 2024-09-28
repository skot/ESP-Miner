import { Injectable } from '@angular/core';
import { BehaviorSubject, Observable } from 'rxjs';
import { finalize, tap } from 'rxjs/operators';

@Injectable({
  providedIn: 'root'
})
export class LoadingService {

  public loading$: BehaviorSubject<boolean> = new BehaviorSubject(false);

  constructor() { }

  public lockUIUntilComplete<T>(): (source: Observable<T>) => Observable<T> {
    return (source: Observable<T>) => source.pipe(
      tap(() => this.loading$.next(true)),
      finalize(() => this.loading$.next(false))
    );
  }
}

