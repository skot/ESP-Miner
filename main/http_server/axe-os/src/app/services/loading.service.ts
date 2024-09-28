import { Injectable } from '@angular/core';
import { BehaviorSubject, Observable, finalize } from 'rxjs';

@Injectable({
  providedIn: 'root'
})
export class LoadingService {

  public loading$: BehaviorSubject<boolean> = new BehaviorSubject(false);

  constructor() { }

  public lockUIUntilComplete() {
    return <T>(source: Observable<T>): Observable<T> => {
      return new Observable(subscriber => {
        this.loading$.next(true);
        const subscription = source.pipe(
          finalize(() => {
            this.loading$.next(false);
          })
        ).subscribe({
          next: (value) => subscriber.next(value),
          error: (err) => subscriber.error(err),
          complete: () => subscriber.complete()
        });

        return () => {
          subscription.unsubscribe();
        };
      });
    }
  }
}

