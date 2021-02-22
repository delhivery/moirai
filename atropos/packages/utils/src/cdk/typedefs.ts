import {
  SecretValue as CdkSecretValue,
  StackProps as CdkStackProps,
  StageProps as CdkStageProps,
} from "@aws-cdk/core";
import { URL } from "url";

export enum EnvironmentType {
  PRODUCTION = "production",
  STAGING = "staging",
  DEVELOPER = "developer",
  LOCAL = "local",
  NULL = "null",
}

export interface IGitHubRepository {
  owner: string;
  repo: string;
  oauthToken: CdkSecretValue;
}

export interface StackProps extends CdkStackProps, CdkStageProps {
  project: string;
  component: string;
  technologyUnit: string;
  environment: EnvironmentType;
  uri: URL;
  [key: string]: any;
}
