import { PageHead } from '@/components/Heads';
import { Page } from '@/components/site/Page';
import { TextLink } from '@/components/TextLink';
import { goSection } from '@/lib/nav';

/** An address the app does not have — the website sends these home. */
export default function NotFoundScreen() {
  return (
    <Page>
      <PageHead kicker="RESTRICTED WEAR" title="NOT FOUND" lede="This page is not available.">
        <TextLink label="RETURN HOME" onPress={() => goSection('home')} style={{ marginTop: 30 }} />
      </PageHead>
    </Page>
  );
}
