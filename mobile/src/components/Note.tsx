import { useWindowDimensions } from 'react-native';

import { fitOneLine } from '@/theme/responsive';

import { T, type TProps } from './T';

/** Below this the note would be too small to read on one line. */
const READABLE = 10;

/**
 * A small-print note made of several sentences. It stays on one line where
 * that is readable; on the narrowest phones each sentence takes its own
 * line instead, so nothing wraps mid-sentence and nothing runs off screen.
 */
export function Note({
  sentences,
  width,
  joiner = ' ',
  size = 12.5,
  ...rest
}: Omit<TProps, 'children' | 'fit' | 'wrap'> & { sentences: string[]; width: number; joiner?: string }) {
  const { fontScale } = useWindowDimensions();
  const joined = sentences.join(joiner);
  const tracking = rest.tracking ?? 0.1;
  const fitted = fitOneLine({ text: joined, max: size, width, weight: 'medium', tracking, fontScale, maxScale: 1.2 });
  const lines = fitted >= Math.min(size, READABLE) ? [joined] : sentences;
  return (
    <>
      {lines.map((line, i) => (
        <T key={line} variant="label" size={size} fit={width} {...rest} style={i === 0 ? rest.style : undefined}>
          {line}
        </T>
      ))}
    </>
  );
}
